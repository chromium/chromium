// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanager/password_credential.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class PasswordCredentialTest : public PageTestBase {
 protected:
  void SetUp() override { PageTestBase::SetUp(IntSize()); }

  HTMLFormElement* PopulateForm(const char* enctype, const char* html) {
    StringBuilder b;
    b.Append("<!DOCTYPE html><html><body><form id='theForm' enctype='");
    b.Append(enctype);
    b.Append("'>");
    b.Append(html);
    b.Append("</form></body></html>");
    SetHtmlInnerHTML(b.ToString().Utf8());
    auto* form = To<HTMLFormElement>(GetElementById("theForm"));
    EXPECT_NE(nullptr, form);
    return form;
  }
};

TEST_F(PasswordCredentialTest, CreateFromMultipartForm) {
  HTMLFormElement* form =
      PopulateForm("multipart/form-data",
                   "<input type='text' name='theId' value='musterman' "
                   "autocomplete='username'>"
                   "<input type='text' name='thePassword' value='sekrit' "
                   "autocomplete='current-password'>"
                   "<input type='text' name='theIcon' "
                   "value='https://example.com/photo' autocomplete='photo'>"
                   "<input type='text' name='theExtraField' value='extra'>"
                   "<input type='text' name='theName' value='friendly name' "
                   "autocomplete='name'>");
  PasswordCredential* credential =
      PasswordCredential::Create(form, ASSERT_NO_EXCEPTION);
  ASSERT_NE(nullptr, credential);

  EXPECT_EQ("musterman", credential->id());
  EXPECT_EQ("sekrit", credential->password());
  EXPECT_EQ(KURL("https://example.com/photo"), credential->iconURL());
  EXPECT_EQ("friendly name", credential->name());
  EXPECT_EQ("password", credential->type());
}

TEST_F(PasswordCredentialTest, CreateFromURLEncodedForm) {
  HTMLFormElement* form =
      PopulateForm("application/x-www-form-urlencoded",
                   "<input type='text' name='theId' value='musterman' "
                   "autocomplete='username'>"
                   "<input type='text' name='thePassword' value='sekrit' "
                   "autocomplete='current-password'>"
                   "<input type='text' name='theIcon' "
                   "value='https://example.com/photo' autocomplete='photo'>"
                   "<input type='text' name='theExtraField' value='extra'>"
                   "<input type='text' name='theName' value='friendly name' "
                   "autocomplete='name'>");
  PasswordCredential* credential =
      PasswordCredential::Create(form, ASSERT_NO_EXCEPTION);
  ASSERT_NE(nullptr, credential);

  EXPECT_EQ("musterman", credential->id());
  EXPECT_EQ("sekrit", credential->password());
  EXPECT_EQ(KURL("https://example.com/photo"), credential->iconURL());
  EXPECT_EQ("friendly name", credential->name());
  EXPECT_EQ("password", credential->type());
}

TEST_F(PasswordCredentialTest, CreateFromFormNoPassword) {
  HTMLFormElement* form =
      PopulateForm("multipart/form-data",
                   "<input type='text' name='theId' value='musterman' "
                   "autocomplete='username'>"
                   "<!-- No password field -->"
                   "<input type='text' name='theIcon' "
                   "value='https://example.com/photo' autocomplete='photo'>"
                   "<input type='text' name='theName' value='friendly name' "
                   "autocomplete='name'>");
  DummyExceptionStateForTesting exception_state;
  PasswordCredential* credential =
      PasswordCredential::Create(form, exception_state);
  EXPECT_EQ(nullptr, credential);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kTypeError, exception_state.CodeAs<ESErrorType>());
  EXPECT_EQ("'password' must not be empty.", exception_state.Message());
}

TEST_F(PasswordCredentialTest, CreateFromFormNoId) {
  HTMLFormElement* form =
      PopulateForm("multipart/form-data",
                   "<!-- No username field. -->"
                   "<input type='text' name='thePassword' value='sekrit' "
                   "autocomplete='current-password'>"
                   "<input type='text' name='theIcon' "
                   "value='https://example.com/photo' autocomplete='photo'>"
                   "<input type='text' name='theName' value='friendly name' "
                   "autocomplete='name'>");
  DummyExceptionStateForTesting exception_state;
  PasswordCredential* credential =
      PasswordCredential::Create(form, exception_state);
  EXPECT_EQ(nullptr, credential);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kTypeError, exception_state.CodeAs<ESErrorType>());
  EXPECT_EQ("'id' must not be empty.", exception_state.Message());
}

}  // namespace blink
