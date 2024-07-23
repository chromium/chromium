// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tests PPB_URLRequestInfo interface.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/tests/test_url_request.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <string>

#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/var.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(URLRequest);

namespace {
// TODO(polina): move these to test_case.h/cc since other NaCl tests use them?

const PP_Resource kInvalidResource = 0;
const PP_Instance kInvalidInstance = 0;

// These should not exist.
// The bottom 2 bits are used to differentiate between different id types.
// 00 - module, 01 - instance, 10 - resource, 11 - var.
const PP_Instance kNotAnInstance = 0xFFFFF0;
const PP_Resource kNotAResource = 0xAAAAA0;
}

TestURLRequest::TestURLRequest(TestingInstance* instance)
    : TestCase(instance),
      ppb_url_request_interface_(NULL),
      ppb_url_loader_interface_(NULL),
      ppb_url_response_interface_(NULL),
      ppb_core_interface_(NULL),
      ppb_var_interface_(NULL) {
}

bool TestURLRequest::Init() {
  ppb_url_request_interface_ = static_cast<const PPB_URLRequestInfo*>(
      pp::Module::Get()->GetBrowserInterface(PPB_URLREQUESTINFO_INTERFACE));
  ppb_url_loader_interface_ = static_cast<const PPB_URLLoader*>(
      pp::Module::Get()->GetBrowserInterface(PPB_URLLOADER_INTERFACE));
  ppb_url_response_interface_ = static_cast<const PPB_URLResponseInfo*>(
      pp::Module::Get()->GetBrowserInterface(PPB_URLRESPONSEINFO_INTERFACE));
  ppb_core_interface_ = static_cast<const PPB_Core*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CORE_INTERFACE));
  ppb_var_interface_ = static_cast<const PPB_Var*>(
      pp::Module::Get()->GetBrowserInterface(PPB_VAR_INTERFACE));
  if (!ppb_url_request_interface_)
    instance_->AppendError("PPB_URLRequestInfo interface not available");
  if (!ppb_url_response_interface_)
    instance_->AppendError("PPB_URLResponseInfo interface not available");
  if (!ppb_core_interface_)
    instance_->AppendError("PPB_Core interface not available");
  if (!ppb_var_interface_)
    instance_->AppendError("PPB_Var interface not available");
  if (!ppb_url_loader_interface_) {
    instance_->AppendError("PPB_URLLoader interface not available");
  }
  return EnsureRunningOverHTTP();
}

void TestURLRequest::RunTests(const std::string& filter) {
  RUN_TEST(CreateAndIsURLRequestInfo, filter);
  RUN_TEST(SetProperty, filter);
  RUN_TEST(AppendDataToBody, filter);
  RUN_TEST(AppendFileToBody, filter);
  RUN_TEST(Stress, filter);
}

PP_Var TestURLRequest::PP_MakeString(const char* s) {
  return ppb_var_interface_->VarFromUtf8(s, static_cast<int32_t>(strlen(s)));
}

// Tests
//   PP_Resource Create(PP_Instance instance)
//   PP_Bool IsURLRequestInfo(PP_Resource resource)
std::string TestURLRequest::TestCreateAndIsURLRequestInfo() {
  // Create: Invalid / non-existent instance -> invalid resource.
  ASSERT_EQ(ppb_url_request_interface_->Create(kInvalidInstance),
            kInvalidResource);
  ASSERT_EQ(ppb_url_request_interface_->Create(kNotAnInstance),
            kInvalidResource);

  // Create: Valid instance -> valid resource.
  PP_Resource url_request = ppb_url_request_interface_->Create(
      instance_->pp_instance());
  ASSERT_NE(url_request, kInvalidResource);

  // IsURLRequestInfo:
  // Invalid / non-existent / non-URLRequestInfo resource -> false.
  ASSERT_NE(PP_TRUE,
            ppb_url_request_interface_->IsURLRequestInfo(kInvalidResource));
  ASSERT_NE(PP_TRUE,
            ppb_url_request_interface_->IsURLRequestInfo(kNotAResource));

  PP_Resource url_loader =
      ppb_url_loader_interface_->Create(instance_->pp_instance());
  ASSERT_NE(kInvalidResource, url_loader);

  ASSERT_NE(PP_TRUE, ppb_url_request_interface_->IsURLRequestInfo(url_loader));
  ppb_url_loader_interface_->Close(url_loader);
  ppb_core_interface_->ReleaseResource(url_loader);

  // IsURLRequestInfo: Current URLRequestInfo resource -> true.
  std::string error;
  if (PP_FALSE == ppb_url_request_interface_->IsURLRequestInfo(url_request))
    error = "IsURLRequestInfo() failed with a current URLRequestInfo resource";

  // IsURLRequestInfo: Released URLRequestInfo resource -> false.
  ppb_core_interface_->ReleaseResource(url_request);
  ASSERT_NE(PP_TRUE, ppb_url_request_interface_->IsURLRequestInfo(url_request));

  return error;  // == PASS() if empty.
}

// Tests
//  PP_Bool SetProperty(PP_Resource request,
//                      PP_URLRequestProperty property,
//                      struct PP_Var value);
std::string TestURLRequest::TestSetProperty() {
  struct PropertyTestData {
    PropertyTestData(PP_URLRequestProperty prop,
                     const std::string& name,
                     PP_Var value, PP_Bool expected) :
        property(prop), property_name(name),
        var(value), expected_value(expected) {
      // var has ref count of 1 on creation.
    }
    PP_URLRequestProperty property;
    std::string property_name;
    PP_Var var;  // Instance owner is responsible for releasing this var.
    PP_Bool expected_value;
  };

  // All bool properties should accept PP_TRUE and PP_FALSE, while rejecting
  // all other variable types.
#define TEST_BOOL(_name)                                              \
    PropertyTestData(ID_STR(_name), PP_MakeBool(PP_TRUE), PP_TRUE),   \
    PropertyTestData(ID_STR(_name), PP_MakeBool(PP_FALSE), PP_TRUE),  \
    PropertyTestData(ID_STR(_name), PP_MakeUndefined(), PP_FALSE),    \
    PropertyTestData(ID_STR(_name), PP_MakeNull(), PP_FALSE),         \
    PropertyTestData(ID_STR(_name), PP_MakeInt32(0), PP_FALSE),       \
    PropertyTestData(ID_STR(_name), PP_MakeDouble(0.0), PP_FALSE)

  // These property types are always invalid for string properties.
#define TEST_STRING_INVALID(_name)                                    \
    PropertyTestData(ID_STR(_name), PP_MakeNull(), PP_FALSE),         \
    PropertyTestData(ID_STR(_name), PP_MakeBool(PP_FALSE), PP_FALSE), \
    PropertyTestData(ID_STR(_name), PP_MakeInt32(0), PP_FALSE),       \
    PropertyTestData(ID_STR(_name), PP_MakeDouble(0.0), PP_FALSE)

#define TEST_INT_INVALID(_name)                                         \
    PropertyTestData(ID_STR(_name), PP_MakeUndefined(), PP_FALSE),      \
    PropertyTestData(ID_STR(_name), PP_MakeNull(), PP_FALSE),           \
    PropertyTestData(ID_STR(_name), PP_MakeBool(PP_FALSE), PP_FALSE),   \
    PropertyTestData(ID_STR(_name), PP_MakeString("notint"), PP_FALSE), \
    PropertyTestData(ID_STR(_name), PP_MakeDouble(0.0), PP_FALSE)

  // SetProperty accepts plenty of invalid values (malformed urls, negative
  // thresholds, etc). Error checking is delayed until request opening (aka url
  // loading).
#define ID_STR(arg) arg, #arg
  PropertyTestData test_data[] = {
      TEST_INT_INVALID(PP_URLREQUESTPROPERTY_STREAMTOFILE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_STREAMTOFILE),
                       PP_MakeBool(PP_TRUE), PP_FALSE),
      TEST_BOOL(PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS),
      TEST_BOOL(PP_URLREQUESTPROPERTY_RECORDDOWNLOADPROGRESS),
      TEST_BOOL(PP_URLREQUESTPROPERTY_RECORDUPLOADPROGRESS),
      TEST_BOOL(PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS),
      TEST_BOOL(PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS),
      TEST_STRING_INVALID(PP_URLREQUESTPROPERTY_URL),
      TEST_STRING_INVALID(PP_URLREQUESTPROPERTY_METHOD),
      TEST_STRING_INVALID(PP_URLREQUESTPROPERTY_HEADERS),
      TEST_STRING_INVALID(PP_URLREQUESTPROPERTY_CUSTOMREFERRERURL),
      TEST_STRING_INVALID(PP_URLREQUESTPROPERTY_CUSTOMCONTENTTRANSFERENCODING),
      TEST_STRING_INVALID(PP_URLREQUESTPROPERTY_CUSTOMUSERAGENT),
      TEST_INT_INVALID(PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD),
      TEST_INT_INVALID(PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERTHRESHOLD),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_URL),
                       PP_MakeString("http://www.google.com"), PP_TRUE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_URL),
                       PP_MakeString("foo.jpg"), PP_TRUE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_METHOD),
                       PP_MakeString("GET"), PP_TRUE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_METHOD),
                       PP_MakeString("POST"), PP_TRUE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_HEADERS),
                       PP_MakeString("Accept: text/plain"), PP_TRUE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_HEADERS), PP_MakeString(""),
                       PP_TRUE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_CUSTOMREFERRERURL),
                       PP_MakeString("http://www.google.com"), PP_TRUE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_CUSTOMREFERRERURL),
                       PP_MakeString(""), PP_TRUE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_CUSTOMREFERRERURL),
                       PP_MakeUndefined(), PP_TRUE),
      PropertyTestData(
          ID_STR(PP_URLREQUESTPROPERTY_CUSTOMCONTENTTRANSFERENCODING),
          PP_MakeString("base64"), PP_TRUE),
      PropertyTestData(
          ID_STR(PP_URLREQUESTPROPERTY_CUSTOMCONTENTTRANSFERENCODING),
          PP_MakeString(""), PP_TRUE),
      PropertyTestData(
          ID_STR(PP_URLREQUESTPROPERTY_CUSTOMCONTENTTRANSFERENCODING),
          PP_MakeUndefined(), PP_TRUE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_CUSTOMUSERAGENT),
                       PP_MakeString("My Crazy Plugin"), PP_TRUE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_CUSTOMUSERAGENT),
                       PP_MakeString(""), PP_TRUE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_CUSTOMUSERAGENT),
                       PP_MakeUndefined(), PP_TRUE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_URL), PP_MakeUndefined(),
                       PP_FALSE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_METHOD), PP_MakeUndefined(),
                       PP_FALSE),
      PropertyTestData(
          ID_STR(PP_URLREQUESTPROPERTY_HEADERS),
          PP_MakeString("Proxy-Authorization: Basic dXNlcjpwYXNzd29yZA=="),
          PP_TRUE),
      PropertyTestData(
          ID_STR(PP_URLREQUESTPROPERTY_HEADERS),
          PP_MakeString("Accept-Encoding: *\n"
                        "Accept-Charset: iso-8859-5, unicode-1-1;q=0.8"),
          PP_TRUE),
      PropertyTestData(
          ID_STR(PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD),
          PP_MakeInt32(0), PP_TRUE),
      PropertyTestData(
          ID_STR(PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD),
          PP_MakeInt32(100), PP_TRUE),
      PropertyTestData(
          ID_STR(PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERTHRESHOLD),
          PP_MakeInt32(0), PP_TRUE),
      PropertyTestData(
          ID_STR(PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERTHRESHOLD),
          PP_MakeInt32(100), PP_TRUE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_URL),
                       PP_MakeString("::::::::::::"), PP_TRUE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_METHOD),
                       PP_MakeString("INVALID"), PP_TRUE),
      PropertyTestData(
          ID_STR(PP_URLREQUESTPROPERTY_CUSTOMCONTENTTRANSFERENCODING),
          PP_MakeString("invalid"), PP_TRUE),
      PropertyTestData(
          ID_STR(PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD),
          PP_MakeInt32(-100), PP_TRUE),
      PropertyTestData(
          ID_STR(PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERTHRESHOLD),
          PP_MakeInt32(-100), PP_TRUE),

  };
  std::string error;

  PP_Resource url_request = ppb_url_request_interface_->Create(
      instance_->pp_instance());
  if (url_request == kInvalidResource)
    error = "Failed to create a URLRequestInfo";

  // Loop over all test data even if we encountered an error to release vars.
  for (size_t i = 0;
       i < sizeof(test_data) / sizeof(test_data[0]);
       ++i) {
    if (error.empty() && test_data[i].expected_value !=
        ppb_url_request_interface_->SetProperty(url_request,
                                                test_data[i].property,
                                                test_data[i].var)) {
      pp::Var var(pp::Var::DontManage(), test_data[i].var);
      error = std::string("Setting property ") +
          test_data[i].property_name + " to " + var.DebugString() +
          " did not return " + (test_data[i].expected_value ? "True" : "False");
    }
    ppb_var_interface_->Release(test_data[i].var);
  }

  ppb_core_interface_->ReleaseResource(url_request);
  return error;  // == PASS() if empty.
}

std::string TestURLRequest::LoadAndCompareBody(
    PP_Resource url_request, const std::string& expected_body) {
  TestCompletionCallback callback(instance_->pp_instance(), PP_REQUIRED);

  PP_Resource url_loader =
      ppb_url_loader_interface_->Create(instance_->pp_instance());
  ASSERT_NE(kInvalidResource, url_loader);

  callback.WaitForResult(ppb_url_loader_interface_->Open(
      url_loader, url_request,
      callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  std::string error;
  PP_Resource url_response =
      ppb_url_loader_interface_->GetResponseInfo(url_loader);
  if (url_response == kInvalidResource) {
    error = "PPB_URLLoader::GetResponseInfo() returned invalid resource";
  } else {
    PP_Var status = ppb_url_response_interface_->GetProperty(
        url_response, PP_URLRESPONSEPROPERTY_STATUSCODE);
    if (status.type != PP_VARTYPE_INT32 && status.value.as_int != 200)
      error = ReportError("PPB_URLLoader::Open() status", status.value.as_int);

    std::string actual_body;
    for (; error.empty();) {  // Read the entire body in this loop.
      const size_t kBufferSize = 32;
      char buf[kBufferSize];
      callback.WaitForResult(ppb_url_loader_interface_->ReadResponseBody(
          url_loader, buf, kBufferSize,
          callback.GetCallback().pp_completion_callback()));
      if (callback.failed())
        error.assign(callback.errors());
      else if (callback.result() < PP_OK)
        error.assign(ReportError("PPB_URLLoader::ReadResponseBody()",
                                 callback.result()));
      if (callback.result() <= PP_OK || callback.failed())
        break;
      actual_body.append(buf, callback.result());
    }
    if (actual_body != expected_body)
      error = "PPB_URLLoader::ReadResponseBody() read unexpected response.";
  }
  ppb_core_interface_->ReleaseResource(url_response);

  ppb_url_loader_interface_->Close(url_loader);
  ppb_core_interface_->ReleaseResource(url_loader);
  return error;
}

// Tests
//   PP_Bool AppendDataToBody(
//       PP_Resource request, const void* data, uint32_t len);
std::string TestURLRequest::TestAppendDataToBody() {
  PP_Resource url_request = ppb_url_request_interface_->Create(
      instance_->pp_instance());
  ASSERT_NE(url_request, kInvalidResource);

  std::string postdata("sample postdata");
  PP_Var post_string_var = PP_MakeString("POST");
  PP_Var echo_string_var = PP_MakeString("/echo");

  // NULL pointer causes a crash. In general PPAPI implementation does not
  // test for NULL because they are just a special case of bad pointers that
  // are not detectable if set to point to an object that does not exist.

  // Invalid resource should fail.
  ASSERT_EQ(PP_FALSE, ppb_url_request_interface_->AppendDataToBody(
      kInvalidResource, postdata.data(),
      static_cast<uint32_t>(postdata.length())));

  // Append data and POST to echoing web server.
  ASSERT_EQ(PP_TRUE, ppb_url_request_interface_->SetProperty(
      url_request, PP_URLREQUESTPROPERTY_METHOD, post_string_var));
  ASSERT_EQ(PP_TRUE, ppb_url_request_interface_->SetProperty(
      url_request, PP_URLREQUESTPROPERTY_URL, echo_string_var));

  // Append data to body and verify the body is what we expect.
  ASSERT_EQ(PP_TRUE, ppb_url_request_interface_->AppendDataToBody(
      url_request, postdata.data(),
      static_cast<uint32_t>(postdata.length())));
  std::string error = LoadAndCompareBody(url_request, postdata);

  ppb_var_interface_->Release(post_string_var);
  ppb_var_interface_->Release(echo_string_var);
  ppb_core_interface_->ReleaseResource(url_request);
  return error;  // == PASS() if empty.
}

std::string TestURLRequest::TestAppendFileToBody() {
  PP_Resource url_request = ppb_url_request_interface_->Create(
      instance_->pp_instance());
  ASSERT_NE(url_request, kInvalidResource);

  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  pp::FileRef ref(file_system, "/test_file");
  pp::FileIO io(instance_);
  callback.WaitForResult(io.Open(ref,
                                 PP_FILEOPENFLAG_CREATE | PP_FILEOPENFLAG_WRITE,
                                 callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  std::string append_data = "hello\n";
  callback.WaitForResult(io.Write(0,
                                  append_data.c_str(),
                                  static_cast<int32_t>(append_data.size()),
                                  callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(static_cast<int32_t>(append_data.size()), callback.result());

  PP_Var post_string_var = PP_MakeString("POST");
  PP_Var echo_string_var = PP_MakeString("/echo");

  // NULL pointer causes a crash. In general PPAPI implementation does not
  // test for NULL because they are just a special case of bad pointers that
  // are not detectable if set to point to an object that does not exist.

  // Invalid resource should fail.
  ASSERT_EQ(PP_FALSE, ppb_url_request_interface_->AppendFileToBody(
      kInvalidResource, ref.pp_resource(), 0, -1, 0));

  // Append data and POST to echoing web server.
  ASSERT_EQ(PP_TRUE, ppb_url_request_interface_->SetProperty(
      url_request, PP_URLREQUESTPROPERTY_METHOD, post_string_var));
  ASSERT_EQ(PP_TRUE, ppb_url_request_interface_->SetProperty(
      url_request, PP_URLREQUESTPROPERTY_URL, echo_string_var));

  // Append file to body and verify the body is what we expect.
  ASSERT_EQ(PP_TRUE, ppb_url_request_interface_->AppendFileToBody(
      url_request, ref.pp_resource(), 0, -1, 0));
  std::string error = LoadAndCompareBody(url_request, append_data);

  ppb_var_interface_->Release(post_string_var);
  ppb_var_interface_->Release(echo_string_var);
  ppb_core_interface_->ReleaseResource(url_request);
  return error;  // == PASS() if empty.
}

// Allocates and manipulates a large number of resources.
std::string TestURLRequest::TestStress() {
  const int kManyResources = 500;
  PP_Resource url_request_info[kManyResources];

  std::string error;
  int num_created = kManyResources;
  for (int i = 0; i < kManyResources; i++) {
    url_request_info[i] = ppb_url_request_interface_->Create(
        instance_->pp_instance());
    if (url_request_info[i] == kInvalidResource) {
      error = "Create() failed";
    } else if (PP_FALSE == ppb_url_request_interface_->IsURLRequestInfo(
        url_request_info[i])) {
      error = "IsURLRequestInfo() failed";
    } else if (PP_FALSE == ppb_url_request_interface_->SetProperty(
                               url_request_info[i],
                               PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS,
                               PP_MakeBool(PP_TRUE))) {
      error = "SetProperty() failed";
    }
    if (!error.empty()) {
      num_created = i + 1;
      break;
    }
  }
  for (int i = 0; i < num_created; i++) {
    ppb_core_interface_->ReleaseResource(url_request_info[i]);
    if (PP_TRUE ==
        ppb_url_request_interface_->IsURLRequestInfo(url_request_info[i]))
      error = "IsURLREquestInfo() succeeded after release";
  }
  return error;  // == PASS() if empty.
}
