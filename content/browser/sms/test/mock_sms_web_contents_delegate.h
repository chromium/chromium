// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_TEST_MOCK_SMS_WEB_CONTENTS_DELEGATE_H_
#define CONTENT_BROWSER_SMS_TEST_MOCK_SMS_WEB_CONTENTS_DELEGATE_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockSmsWebContentsDelegate : public WebContentsDelegate {
 public:
  MockSmsWebContentsDelegate();
  ~MockSmsWebContentsDelegate() override;

  MOCK_METHOD5(CreateSmsPrompt,
               void(RenderFrameHost*,
                    const url::Origin&,
                    const std::string&,
                    base::OnceCallback<void()> on_confirm,
                    base::OnceCallback<void()> on_cancel));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSmsWebContentsDelegate);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_TEST_MOCK_SMS_WEB_CONTENTS_DELEGATE_H_
