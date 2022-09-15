// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_URL_REQUEST_H_
#define PPAPI_TESTS_TEST_URL_REQUEST_H_

#include <string>

#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/c/ppb_url_response_info.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/tests/test_case.h"

class TestURLRequest : public TestCase {
 public:
  explicit TestURLRequest(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestCreateAndIsURLRequestInfo();
  std::string TestSetProperty();
  std::string TestAppendDataToBody();
  std::string TestAppendFileToBody();
  std::string TestStress();

  // Helpers.
  PP_Var PP_MakeString(const char* s);
  std::string LoadAndCompareBody(PP_Resource url_request,
                                 const std::string& expected_body);

  const PPB_URLRequestInfo* ppb_url_request_interface_;
  const PPB_URLLoader* ppb_url_loader_interface_;
  const PPB_URLResponseInfo* ppb_url_response_interface_;
  const PPB_Core* ppb_core_interface_;
  const PPB_Var* ppb_var_interface_;
};

#endif  // PPAPI_TESTS_TEST_URL_REQUEST_H_
