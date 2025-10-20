// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_CONTENT_BROWSER_TEST_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_PUBLIC_TEST_CONTENT_BROWSER_TEST_CONTENT_BROWSER_CLIENT_H_

#include <string_view>

#include "content/shell/browser/shell_content_browser_client.h"
#include "media/mojo/mojom/speech_recognizer.mojom.h"

namespace content {

// ContentBrowserClient implementation used in content browser tests.
// For tests that need to change the ContentBrowserClient subclass this
// class and it will take care of the registration for you (you don't need
// to call SetBrowserClientForTesting().
class ContentBrowserTestContentBrowserClient
    : public ShellContentBrowserClient {
 public:
  ContentBrowserTestContentBrowserClient();
  ~ContentBrowserTestContentBrowserClient() override;

  void OnNetworkServiceCreated(
      network::mojom::NetworkService* network_service) override;

  media::mojom::AvailabilityStatus
  GetOnDeviceSpeechRecognitionAvailabilityStatus(
      BrowserContext* context,
      const std::string& language) override;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_CONTENT_BROWSER_TEST_CONTENT_BROWSER_CLIENT_H_
