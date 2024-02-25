// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/renderer/web_page_metadata_extraction.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "components/webapps/common/web_page_metadata.mojom.h"
#include "third_party/blink/public/platform/web_icon_sizes_parser.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "url/gurl.h"

using blink::WebDocument;
using blink::WebElement;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebString;

namespace webapps {
namespace {

void AddInstallIcon(const WebElement& link,
                    std::vector<mojom::WebPageIconInfoPtr>* icons) {
  WebString href = link.GetAttribute("href");
  if (href.IsNull() || href.IsEmpty())
    return;

  // Get complete url.
  GURL url = link.GetDocument().CompleteURL(href);
  if (!url.is_valid())
    return;

  mojom::WebPageIconInfoPtr icon_info(mojom::WebPageIconInfo::New());
  if (link.HasAttribute("sizes")) {
    blink::WebVector<gfx::Size> icon_sizes =
        blink::WebIconSizesParser::ParseIconSizes(link.GetAttribute("sizes"));
    if (icon_sizes.size() == 1 && icon_sizes[0].width() != 0 &&
        icon_sizes[0].height() == icon_sizes[0].width()) {
      icon_info->square_size_px = icon_sizes[0].width();
    }
  }
  icon_info->url = url;
  icons->push_back(std::move(icon_info));
}

}  // namespace

mojom::WebPageMetadataPtr ExtractWebPageMetadata(WebLocalFrame* frame) {
  mojom::WebPageMetadataPtr metadata(mojom::WebPageMetadata::New());

  WebDocument document = frame->GetDocument();
  if (document.IsNull())
    return metadata;

  WebElement head = document.Head();
  if (head.IsNull()) {
    return metadata;
  }

  metadata->title = document.Title().Utf16();

  GURL document_url = document.Url();
  for (WebNode child = head.FirstChild(); !child.IsNull();
       child = child.NextSibling()) {
    if (!child.IsElementNode())
      continue;
    WebElement elem = child.To<WebElement>();

    if (elem.HasHTMLTagName("link")) {
      std::string rel = elem.GetAttribute("rel").Utf8();
      // "rel" attribute may use either "icon" or "shortcut icon".
      // see also
      //   <http://en.wikipedia.org/wiki/Favicon>
      //   <http://dev.w3.org/html5/spec/Overview.html#rel-icon>
      //
      // Bookmark apps also support "apple-touch-icon" and
      // "apple-touch-icon-precomposed".
      if (base::EqualsCaseInsensitiveASCII(rel, "icon") ||
          base::EqualsCaseInsensitiveASCII(rel, "shortcut icon") ||
          base::EqualsCaseInsensitiveASCII(rel, "apple-touch-icon") ||
          base::EqualsCaseInsensitiveASCII(rel,
                                           "apple-touch-icon-precomposed")) {
        AddInstallIcon(elem, &metadata->icons);
      }
    } else if (elem.HasHTMLTagName("meta") && elem.HasAttribute("name")) {
      std::string name = elem.GetAttribute("name").Utf8();
      WebString content = elem.GetAttribute("content");
      if (name == "application-name") {
        metadata->application_name = content.Utf16();
      } else if (name == "description") {
        metadata->description = content.Utf16();
      } else if (name == "application-url") {
        std::string url = content.Utf8();
        metadata->application_url =
            document_url.is_valid() ? document_url.Resolve(url) : GURL(url);
        if (!metadata->application_url.is_valid())
          metadata->application_url = GURL();
      } else if (name == "mobile-web-app-capable" &&
                 base::EqualsCaseInsensitiveASCII(content.Utf16(), "yes")) {
        metadata->mobile_capable = mojom::WebPageMobileCapable::ENABLED;
      } else if (name == "apple-mobile-web-app-capable" &&
                 base::EqualsCaseInsensitiveASCII(content.Utf16(), "yes") &&
                 metadata->mobile_capable ==
                     mojom::WebPageMobileCapable::UNSPECIFIED) {
        metadata->mobile_capable = mojom::WebPageMobileCapable::ENABLED_APPLE;
      }
    }
  }

  return metadata;
}

}  // namespace webapps
