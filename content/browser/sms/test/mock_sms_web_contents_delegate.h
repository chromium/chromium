// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_TEST_MOCK_SMS_WEB_CONTENTS_DELEGATE_H_
#define CONTENT_BROWSER_SMS_TEST_MOCK_SMS_WEB_CONTENTS_DELEGATE_H_

#include "content/public/browser/web_contents_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockSmsWebContentsDelegate : public WebContentsDelegate {
 public:
  MockSmsWebContentsDelegate();

  MockSmsWebContentsDelegate(const MockSmsWebContentsDelegate&) = delete;
  MockSmsWebContentsDelegate& operator=(const MockSmsWebContentsDelegate&) =
      delete;

  ~MockSmsWebContentsDelegate() override;

  MOCK_METHOD5(CreateSmsPrompt,
               void(RenderFrameHost*,
                    const std::vector<url::Origin>&,
                    const std::string&,
                    base::OnceCallback<void()> on_confirm,
                    base::OnceCallback<void()> on_cancel));
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_TEST_MOCK_SMS_WEB_CONTENTS_DELEGATE_H_
