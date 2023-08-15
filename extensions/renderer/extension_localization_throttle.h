// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSION_LOCALIZATION_THROTTLE_H_
#define EXTENSIONS_RENDERER_EXTENSION_LOCALIZATION_THROTTLE_H_

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace blink {
class WebURL;
}  // namespace blink

namespace extensions {

// ExtensionLocalizationThrottle is used to pre-process CSS files
// requested by extensions to replace localization templates with the
// appropriate localized strings.
class ExtensionLocalizationThrottle : public blink::URLLoaderThrottle {
 public:
  // Creates a ExtensionLocalizationThrottle only when `request_url`
  // is a chrome-extention scheme URL.
  static std::unique_ptr<ExtensionLocalizationThrottle> MaybeCreate(
      const blink::WebURL& request_url);

  ~ExtensionLocalizationThrottle() override;

  // Implements blink::URLLoaderThrottle.
  void DetachFromCurrentSequence() override;
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;

  // Force CreateDataPipe to result in an error.
  void ForceCreateDataPipeErrorForTest() { force_error_for_test_ = true; }

 private:
  ExtensionLocalizationThrottle();
  void DeferredCancelWithError(int error_code);

  bool force_error_for_test_ = false;
  base::WeakPtrFactory<ExtensionLocalizationThrottle> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSION_LOCALIZATION_THROTTLE_H_
