// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MIME_HANDLER_STREAM_CONTAINER_H_
#define EXTENSIONS_BROWSER_MIME_HANDLER_STREAM_CONTAINER_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/common/api/mime_handler.mojom.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "third_party/blink/public/mojom/loader/transferrable_url_loader.mojom.h"
#include "url/gurl.h"

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace extensions {

class MimeHandlerBodyCache;

// A container for the information necessary for a MimeHandler to handle a
// resource stream.
class StreamContainer {
 public:
  StreamContainer(int tab_id,
                  bool embedded,
                  const GURL& handler_url,
                  const ExtensionId& extension_id,
                  blink::mojom::TransferrableURLLoaderPtr transferrable_loader,
                  const GURL& original_url);

  StreamContainer(const StreamContainer&) = delete;
  StreamContainer& operator=(const StreamContainer&) = delete;

  ~StreamContainer();

  base::WeakPtr<StreamContainer> GetWeakPtr();

  blink::mojom::TransferrableURLLoaderPtr TakeTransferrableURLLoader();

  bool embedded() const { return embedded_; }
  int tab_id() const { return tab_id_; }
  const GURL& handler_url() const { return handler_url_; }
  const ExtensionId& extension_id() const { return extension_id_; }

  const std::string& mime_type() const { return mime_type_; }
  const GURL& original_url() const { return original_url_; }
  const GURL& stream_url() const { return stream_url_; }
  net::HttpResponseHeaders* response_headers() const {
    return response_headers_.get();
  }

  const mime_handler::PdfPluginAttributesPtr& pdf_plugin_attributes() const {
    return pdf_plugin_attributes_;
  }
  void set_pdf_plugin_attributes(
      mime_handler::PdfPluginAttributesPtr pdf_plugin_attributes) {
    pdf_plugin_attributes_ = std::move(pdf_plugin_attributes);
  }

  // Sets the body cache that will accumulate the response body so it can be
  // replayed into a fresh data pipe on fallback.
  void SetBodyCache(scoped_refptr<MimeHandlerBodyCache> cache);

  // Creates a new data pipe consumer containing the cached body data.
  // Returns an invalid handle if no cache is attached or the source has not
  // been fully drained yet.
  mojo::ScopedDataPipeConsumerHandle GetFallbackDataPipe();

 private:
  const bool embedded_;
  const int tab_id_;
  const GURL handler_url_;
  const ExtensionId extension_id_;
  blink::mojom::TransferrableURLLoaderPtr transferrable_loader_;

  std::string mime_type_;
  GURL original_url_;
  GURL stream_url_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  mime_handler::PdfPluginAttributesPtr pdf_plugin_attributes_;
  scoped_refptr<MimeHandlerBodyCache> body_cache_;

  base::WeakPtrFactory<StreamContainer> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MIME_HANDLER_STREAM_CONTAINER_H_
