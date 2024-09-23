// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/app_shim_remote_cocoa/web_drag_source_mac.h"

#include <Cocoa/Cocoa.h>
#include <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <sys/param.h>

#include <memory>
#include <utility>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/pickle.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/download/drag_download_file.h"
#include "content/browser/download/drag_download_util.h"
#include "content/common/web_contents_ns_view_bridge.mojom.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/drop_data.h"
#include "net/base/apple/url_conversions.h"
#include "net/base/filename_util.h"
#include "net/base/mime_util.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#include "url/origin.h"
#include "url/url_constants.h"

@implementation WebDragSource {
  // The host through which to communicate with the WebContents. Owns
  // this object. This pointer gets reset when the WebContents goes away with
  // `webContentsIsGone`.
  raw_ptr<remote_cocoa::mojom::WebContentsNSViewHost> _host;

  // The drop data.
  content::DropData _dropData;

  // The source origin the drop data came from.
  url::Origin _sourceOrigin;

  // Whether to mark the drag as having come from a privileged WebContents.
  BOOL _privileged;

  // The file name to be saved to for a drag-out download.
  base::FilePath _downloadFileName;

  // The URL to download from for a drag-out download.
  GURL _downloadURL;

  // The file type associated with the file drag, if any.
  UTType* __strong _fileType;
}

- (instancetype)initWithHost:(remote_cocoa::mojom::WebContentsNSViewHost*)host
                    dropData:(const content::DropData&)dropData
                sourceOrigin:(const url::Origin&)sourceOrigin
                isPrivileged:(BOOL)privileged {
  if ((self = [super init])) {
    _host = host;
    _dropData = dropData;
    _sourceOrigin = sourceOrigin;
    _privileged = privileged;
  }

  return self;
}

- (void)webContentsIsGone {
  _host = nullptr;
}

- (NSArray<NSPasteboardType>*)writableTypesForPasteboard:
    (NSPasteboard*)pasteboard {
  NSMutableArray<NSPasteboardType>* writableTypes = [NSMutableArray array];

  // Always add kUTTypeChromiumInitiatedDrag to mark this drag as something to
  // accept.
  [writableTypes addObject:ui::kUTTypeChromiumInitiatedDrag];

  // Always add kUTTypeChromiumRendererInitiatedDrag as all drags initiated here
  // are drags from the web.
  [writableTypes addObject:ui::kUTTypeChromiumRendererInitiatedDrag];

  // Tag the drag as coming from a privileged WebContents if needed.
  if (_privileged) {
    [writableTypes addObject:ui::kUTTypeChromiumPrivilegedInitiatedDrag];
  }

  // URL (and title).
  if (_dropData.url.is_valid()) {
    [writableTypes addObject:NSPasteboardTypeURL];
    [writableTypes addObject:ui::kUTTypeURLName];
  }

  // File.
  if (!_dropData.file_contents.empty() ||
      !_dropData.download_metadata.empty()) {
    std::string mimeType;

    // TODO(crbug.com/40599578): The |downloadFileName_| and
    // |downloadURL_| values should be computed by the caller.
    if (_dropData.download_metadata.empty()) {
      std::optional<base::FilePath> suggestedFilename =
          _dropData.GetSafeFilenameForImageFileContents();
      if (suggestedFilename) {
        _downloadFileName = std::move(*suggestedFilename);
        net::GetMimeTypeFromFile(_downloadFileName, &mimeType);
      }
    } else {
      std::u16string mimeType16;
      base::FilePath filename;
      if (content::ParseDownloadMetadata(_dropData.download_metadata,
                                         &mimeType16, &filename,
                                         &_downloadURL)) {
        // Generate the file name based on both mime type and proposed file
        // name.
        std::string defaultName = content::GetContentClient()->browser()
                                      ? content::GetContentClient()
                                            ->browser()
                                            ->GetDefaultDownloadName()
                                      : std::string();
        mimeType = base::UTF16ToUTF8(mimeType16);
        _downloadFileName =
            net::GenerateFileName(_downloadURL, std::string(), std::string(),
                                  filename.value(), mimeType, defaultName);
      }
    }

    if (!mimeType.empty()) {
      _fileType = [UTType typeWithMIMEType:base::SysUTF8ToNSString(mimeType)];

      // Promise both the file's contents...
      if (!_dropData.file_contents.empty()) {
        [writableTypes addObject:_fileType.identifier];
      }

      // ... and materialization of the file if requested.

      // NB: Why not use `NSFilePromiseProvider`? Its design is fundamentally
      // broken. It insists on being added to the pasteboard as its own object,
      // but this code needs to add many, many flavors as one object. The only
      // way to get it to share a pasteboard item with other flavors is to play
      // the game of subclassing it, but that would involve a big rewrite of all
      // of this code. FB11876926
      //
      // https://buckleyisms.com/blog/how-to-actually-implement-file-dragging-from-your-app-on-mac/

      [writableTypes
          addObject:base::apple::CFToNSPtrCast(kPasteboardTypeFileURLPromise)];
      [writableTypes addObject:base::apple::CFToNSPtrCast(
                                   kPasteboardTypeFilePromiseContent)];
    }
  }

  // HTML.
  bool hasHTMLData = _dropData.html && !_dropData.html->empty();
  // Mail.app and TextEdit accept drags that have both HTML and image flavors on
  // them, but don't process them correctly <http://crbug.com/55879>. Therefore,
  // if there is an image flavor, don't put the HTML data on as HTML, but rather
  // put it on as this Chrome-only flavor.
  //
  // (The only time that Blink fills in the DropData::file_contents is with
  // an image drop, but the MIME time is tested anyway for paranoia's sake.)
  bool hasImageData = !_dropData.file_contents.empty() && _fileType &&
                      [_fileType conformsToType:UTTypeImage];
  if (hasHTMLData) {
    if (hasImageData) {
      [writableTypes addObject:ui::kUTTypeChromiumImageAndHTML];
    } else {
      [writableTypes addObject:NSPasteboardTypeHTML];
    }
  }

  // Plain text.
  if (_dropData.text && !_dropData.text->empty()) {
    [writableTypes addObject:NSPasteboardTypeString];
  }

  if (!_dropData.custom_data.empty()) {
    [writableTypes addObject:ui::kUTTypeChromiumDataTransferCustomData];
  }

  return writableTypes;
}

- (id)pasteboardPropertyListForType:(NSPasteboardType)type {
  // HTML.
  if ([type isEqualToString:NSPasteboardTypeHTML] ||
      [type isEqualToString:ui::kUTTypeChromiumImageAndHTML]) {
    DCHECK(_dropData.html && !_dropData.html->empty());

    // NSPasteboardTypeHTML requires the character set to be declared.
    // Otherwise, it assumes US-ASCII. Awesome.
    static constexpr char16_t kHtmlHeader[] =
        u"<meta http-equiv=\"Content-Type\" "
        u"content=\"text/html;charset=UTF-8\">";
    return base::SysUTF16ToNSString(kHtmlHeader + *_dropData.html);
  }

  // URL.
  if ([type isEqualToString:NSPasteboardTypeURL]) {
    DCHECK(_dropData.url.is_valid());
    NSURL* url = net::NSURLWithGURL(_dropData.url);
    // If NSURL creation failed, check for a badly-escaped JavaScript URL.
    // Strip out any existing escapes and then re-escape uniformly.
    if (!url && _dropData.url.SchemeIs(url::kJavaScriptScheme)) {
      std::string unescapedUrlString =
          base::UnescapeBinaryURLComponent(_dropData.url.spec());
      std::string escapedUrlString =
          base::EscapeUrlEncodedData(unescapedUrlString, false);
      url = [NSURL URLWithString:base::SysUTF8ToNSString(escapedUrlString)];
    }
    return url.absoluteString;
  }

  // URL title.
  if ([type isEqualToString:ui::kUTTypeURLName]) {
    return base::SysUTF16ToNSString(_dropData.url_title);
  }

  // File contents.
  if ([type isEqualToString:_fileType.identifier]) {
    return [NSData dataWithBytes:_dropData.file_contents.data()
                          length:_dropData.file_contents.length()];
  }

  // File instantiation promise.
  if ([type isEqualToString:base::apple::CFToNSPtrCast(
                                kPasteboardTypeFilePromiseContent)]) {
    return _fileType.identifier;
  }
  if ([type isEqualToString:base::apple::CFToNSPtrCast(
                                kPasteboardTypeFileURLPromise)]) {
    // The official way of getting the drop destination is to call
    // `PasteboardCopyPasteLocation` on the Carbon Pasteboard Manager, but what
    // that function does is pull the location from "com.apple.pastelocation".
    // Therefore, do that directly rather than indirecting to a different API
    // set that does no useful bridging.
    NSPasteboard* pasteboard =
        [NSPasteboard pasteboardWithName:NSPasteboardNameDrag];
    NSString* dropDestination =
        [pasteboard stringForType:@"com.apple.pastelocation"];
    if (!dropDestination || !_host) {
      // Something has gone wrong, but understandably. Chromium leaves the data
      // around on the pasteboard after the drag, and it's possible that some
      // app is rummaging around for what it can find. Silently fail in this
      // case.
      return [NSData data];
    }

    base::FilePath filePath =
        base::apple::NSURLToFilePath([NSURL URLWithString:dropDestination]);
    filePath = filePath.Append(_downloadFileName);
    _host->DragPromisedFileTo(filePath, _dropData, _downloadURL, _sourceOrigin,
                              &filePath);

    // The process of writing the file may have altered the value of
    // `filePath` if, say, an existing file at the drop site already had that
    // name. Return the actual URL to the file that was written.
    return base::apple::FilePathToNSURL(filePath).absoluteString;
  }

  // Plain text.
  if ([type isEqualToString:NSPasteboardTypeString]) {
    DCHECK(_dropData.text && !_dropData.text->empty());
    return base::SysUTF16ToNSString(*_dropData.text);
  }

  // Custom MIME data.
  if ([type isEqualToString:ui::kUTTypeChromiumDataTransferCustomData]) {
    base::Pickle pickle;
    ui::WriteCustomDataToPickle(_dropData.custom_data, &pickle);
    return [NSData dataWithBytes:pickle.data() length:pickle.size()];
  }

  // Source origin of the drop data.
  if ([type isEqualToString:ui::kUTTypeChromiumRendererInitiatedDrag]) {
    return _sourceOrigin.opaque()
               ? [NSString string]
               : base::SysUTF8ToNSString(_sourceOrigin.Serialize());
  }

  // Flavors used to tag.
  if ([type isEqualToString:ui::kUTTypeChromiumInitiatedDrag] ||
      [type isEqualToString:ui::kUTTypeChromiumPrivilegedInitiatedDrag]) {
    // The type _was_ promised and someone decided to call the bluff.
    return [NSData data];
  }

  // Oops! Unknown drag pasteboard type.
  NOTREACHED_IN_MIGRATION();
  return [NSData data];
}

@end  // @implementation WebDragSource
