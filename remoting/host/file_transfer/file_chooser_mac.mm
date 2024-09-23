// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/file_chooser.h"

#import <Cocoa/Cocoa.h>

#include <utility>

#include "base/apple/foundation_util.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#import "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "remoting/base/string_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

@interface FileTransferOpenPanelDelegate : NSObject <NSOpenSavePanelDelegate> {
}
- (BOOL)panel:(id)sender shouldEnableURL:(NSURL*)url;
- (BOOL)panel:(id)sender validateURL:(NSURL*)url error:(NSError**)outError;
@end

@implementation FileTransferOpenPanelDelegate
- (BOOL)panel:(id)sender shouldEnableURL:(NSURL*)url {
  return url.fileURL;
}

- (BOOL)panel:(id)sender validateURL:(NSURL*)url error:(NSError**)outError {
  // Refuse to accept users closing the dialog with a key repeat, since the key
  // may have been first pressed while the user was looking at something else.
  if (NSApp.currentEvent.type == NSEventTypeKeyDown &&
      NSApp.currentEvent.ARepeat) {
    return NO;
  }

  return YES;
}
@end

namespace remoting {

namespace {

class FileChooserMac;

class MacFileChooserOnUiThread {
 public:
  MacFileChooserOnUiThread(
      scoped_refptr<base::SequencedTaskRunner> caller_task_runner,
      base::WeakPtr<FileChooserMac> file_chooser_mac);

  MacFileChooserOnUiThread(const MacFileChooserOnUiThread&) = delete;
  MacFileChooserOnUiThread& operator=(const MacFileChooserOnUiThread&) = delete;

  ~MacFileChooserOnUiThread();

  void Show();

 private:
  void RunCallback(FileChooser::Result result);

  FileTransferOpenPanelDelegate* __strong delegate_;
  NSOpenPanel* __strong open_panel_;
  scoped_refptr<base::SequencedTaskRunner> caller_task_runner_;
  base::WeakPtr<FileChooserMac> file_chooser_mac_;
  base::WeakPtrFactory<MacFileChooserOnUiThread> weak_ptr_factory_;
};

class FileChooserMac : public FileChooser {
 public:
  FileChooserMac(scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
                 ResultCallback callback);

  FileChooserMac(const FileChooserMac&) = delete;
  FileChooserMac& operator=(const FileChooserMac&) = delete;

  ~FileChooserMac() override;

  // FileChooser implementation.
  void Show() override;

  void RunCallback(FileChooser::Result result);

 private:
  FileChooser::ResultCallback callback_;
  base::SequenceBound<MacFileChooserOnUiThread> mac_file_chooser_on_ui_thread_;
  base::WeakPtrFactory<FileChooserMac> weak_ptr_factory_;
};

MacFileChooserOnUiThread::MacFileChooserOnUiThread(
    scoped_refptr<base::SequencedTaskRunner> caller_task_runner,
    base::WeakPtr<FileChooserMac> file_chooser_mac)
    : delegate_([[FileTransferOpenPanelDelegate alloc] init]),
      caller_task_runner_(std::move(caller_task_runner)),
      file_chooser_mac_(std::move(file_chooser_mac)),
      weak_ptr_factory_(this) {}

MacFileChooserOnUiThread::~MacFileChooserOnUiThread() {
  if (open_panel_) {
    // Note: In existing implementations, this synchronously invokes the
    // completion handler.
    [open_panel_ cancel:open_panel_];
  }
}

void MacFileChooserOnUiThread::Show() {
  DCHECK(!open_panel_);
  open_panel_ = [NSOpenPanel openPanel];
  open_panel_.message = l10n_util::GetNSString(IDS_DOWNLOAD_FILE_DIALOG_TITLE);
  open_panel_.allowsMultipleSelection = NO;
  open_panel_.canChooseFiles = YES;
  open_panel_.canChooseDirectories = NO;
  open_panel_.delegate = delegate_;

  // Because the MacFileChooserOnUiThread destructor calls `cancel` on the open
  // panel if it is still open, which in turn causes the completion handler to
  // be invoked synchronously, weak_this is expected always to be valid.
  // However, because `cancel` does not appear to be explicitly documented to
  // invoke the completion handler synchronously, using a weak pointer guards
  // against any hypothetical future change in behavior.
  base::WeakPtr<MacFileChooserOnUiThread> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  [open_panel_ beginWithCompletionHandler:^(NSModalResponse result) {
    if (!weak_this) {
      return;
    }
    if (result == NSModalResponseOK) {
      NSURL* url = weak_this->open_panel_.URLs[0];
      if (url.fileURL) {
        weak_this->RunCallback(base::apple::NSStringToFilePath([url path]));
      } else {
        // The panel delegate should prevent the user making a selection where
        // `url.fileURL` is false, so this is unexpected.
        weak_this->RunCallback(protocol::MakeFileTransferError(
            FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
      }
    } else {
      weak_this->RunCallback(protocol::MakeFileTransferError(
          FROM_HERE, protocol::FileTransfer_Error_Type_CANCELED));
    }
    weak_this->open_panel_ = nil;
  }];
  // Bring to front.
  [NSApp activateIgnoringOtherApps:YES];
}

void MacFileChooserOnUiThread::RunCallback(FileChooser::Result result) {
  caller_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FileChooserMac::RunCallback, file_chooser_mac_,
                                std::move(result)));
}

FileChooserMac::FileChooserMac(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    ResultCallback callback)
    : callback_(std::move(callback)), weak_ptr_factory_(this) {
  mac_file_chooser_on_ui_thread_ =
      base::SequenceBound<MacFileChooserOnUiThread>(
          ui_task_runner, base::SequencedTaskRunner::GetCurrentDefault(),
          weak_ptr_factory_.GetWeakPtr());
}

void FileChooserMac::Show() {
  mac_file_chooser_on_ui_thread_.AsyncCall(&MacFileChooserOnUiThread::Show);
}

void FileChooserMac::RunCallback(FileChooser::Result result) {
  std::move(callback_).Run(std::move(result));
}

FileChooserMac::~FileChooserMac() = default;

}  // namespace

std::unique_ptr<FileChooser> FileChooser::Create(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    ResultCallback callback) {
  return std::make_unique<FileChooserMac>(std::move(ui_task_runner),
                                          std::move(callback));
}

}  // namespace remoting
