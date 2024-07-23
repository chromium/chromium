// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/tests/test_url_loader.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/cpp/dev/url_util_dev.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/file_io_private.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(URLLoader);

namespace {

int32_t WriteEntireBuffer(PP_Instance instance,
                          pp::FileIO* file_io,
                          int32_t offset,
                          const std::string& data,
                          CallbackType callback_type) {
  TestCompletionCallback callback(instance, callback_type);
  int32_t write_offset = offset;
  const char* buf = data.c_str();
  int32_t size = static_cast<int32_t>(data.size());

  while (write_offset < offset + size) {
    callback.WaitForResult(file_io->Write(write_offset,
                                          &buf[write_offset - offset],
                                          size - write_offset + offset,
                                          callback.GetCallback()));
    if (callback.result() < 0)
      return callback.result();
    if (callback.result() == 0)
      return PP_ERROR_FAILED;
    write_offset += callback.result();
  }

  return PP_OK;
}

}  // namespace

TestURLLoader::TestURLLoader(TestingInstance* instance)
    : TestCase(instance),
      file_io_private_interface_(NULL),
      url_loader_trusted_interface_(NULL) {
}

bool TestURLLoader::Init() {
  if (!CheckTestingInterface()) {
    instance_->AppendError("Testing interface not available");
    return false;
  }

  const PPB_FileIO* file_io_interface = static_cast<const PPB_FileIO*>(
      pp::Module::Get()->GetBrowserInterface(PPB_FILEIO_INTERFACE));
  if (!file_io_interface)
    instance_->AppendError("FileIO interface not available");

  file_io_private_interface_ = static_cast<const PPB_FileIO_Private*>(
      pp::Module::Get()->GetBrowserInterface(PPB_FILEIO_PRIVATE_INTERFACE));
  if (!file_io_private_interface_)
    instance_->AppendError("FileIO_Private interface not available");
  url_loader_trusted_interface_ = static_cast<const PPB_URLLoaderTrusted*>(
      pp::Module::Get()->GetBrowserInterface(PPB_URLLOADERTRUSTED_INTERFACE));
  if (!testing_interface_->IsOutOfProcess()) {
    // Trusted interfaces are not supported under NaCl.
#if !(defined __native_client__)
    if (!url_loader_trusted_interface_)
      instance_->AppendError("URLLoaderTrusted interface not available");
#else
    if (url_loader_trusted_interface_)
      instance_->AppendError("URLLoaderTrusted interface is supported by NaCl");
#endif
  }
  return EnsureRunningOverHTTP();
}

/*
 * The test order is important here, as running tests out of order may cause
 * test timeout.
 *
 * Here is the environment:
 *
 * 1. net::EmbeddedTestServer only accepts one open connection at the time.
 * 2. HTTP socket pool keeps sockets open for several seconds after last use
 * (hoping that there will be another request that could reuse the connection).
 * 3. HTTP socket pool is separated by host/port and privacy mode (which is
 * based on cookies set/get permissions). So, connections to 127.0.0.1,
 * localhost and localhost in privacy mode cannot reuse existing socket and will
 * try to open another connection.
 *
 * Here is the problem:
 *
 * Original test order was repeatedly accessing 127.0.0.1, localhost and
 * localhost in privacy mode, causing new sockets to open and try to connect to
 * testserver, which they couldn't until previous connection is closed by socket
 * pool idle socket timeout (10 seconds).
 *
 * Because of this the test run was taking around 45 seconds, and test was
 * reported as 'timed out' by trybot.
 *
 * Re-ordering of tests provides more sequential access to 127.0.0.1, localhost
 * and localhost in privacy mode. It decreases the number of times when socket
 * pool doesn't have existing connection to host and has to wait, therefore
 * reducing total test time and ensuring its completion under 30 seconds.
 */
void TestURLLoader::RunTests(const std::string& filter) {
  // These tests connect to 127.0.0.1:
  RUN_CALLBACK_TEST(TestURLLoader, BasicGET, filter);
  RUN_CALLBACK_TEST(TestURLLoader, BasicPOST, filter);
  RUN_CALLBACK_TEST(TestURLLoader, BasicFilePOST, filter);
  RUN_CALLBACK_TEST(TestURLLoader, BasicFileRangePOST, filter);
  RUN_CALLBACK_TEST(TestURLLoader, CompoundBodyPOST, filter);
  RUN_CALLBACK_TEST(TestURLLoader, EmptyDataPOST, filter);
  RUN_CALLBACK_TEST(TestURLLoader, BinaryDataPOST, filter);
  RUN_CALLBACK_TEST(TestURLLoader, CustomRequestHeader, filter);
  RUN_CALLBACK_TEST(TestURLLoader, FailsBogusContentLength, filter);
  RUN_CALLBACK_TEST(TestURLLoader, StreamToFile, filter);
  RUN_CALLBACK_TEST(TestURLLoader, UntrustedJavascriptURLRestriction, filter);
  RUN_CALLBACK_TEST(TestURLLoader, TrustedJavascriptURLRestriction, filter);
  RUN_CALLBACK_TEST(TestURLLoader, UntrustedHttpRequests, filter);
  RUN_CALLBACK_TEST(TestURLLoader, TrustedHttpRequests, filter);
  RUN_CALLBACK_TEST(TestURLLoader, FollowURLRedirect, filter);
  RUN_CALLBACK_TEST(TestURLLoader, AuditURLRedirect, filter);
  RUN_CALLBACK_TEST(TestURLLoader, RestrictURLRedirectCommon, filter);
  RUN_CALLBACK_TEST(TestURLLoader, RestrictURLRedirectEnabled, filter);
  RUN_CALLBACK_TEST(TestURLLoader, RestrictURLRedirectDisabled, filter);
  RUN_CALLBACK_TEST(TestURLLoader, AbortCalls, filter);
  RUN_CALLBACK_TEST(TestURLLoader, UntendedLoad, filter);
  RUN_CALLBACK_TEST(TestURLLoader, PrefetchBufferThreshold, filter);
  RUN_CALLBACK_TEST(TestURLLoader, XRequestedWithHeader, filter);
  // These tests connect to localhost with privacy mode enabled:
  RUN_CALLBACK_TEST(TestURLLoader, UntrustedSameOriginRestriction, filter);
  RUN_CALLBACK_TEST(TestURLLoader, UntrustedCrossOriginRequest, filter);
  RUN_CALLBACK_TEST(TestURLLoader, UntrustedCorbEligibleRequest, filter);
  // These tests connect to localhost with privacy mode disabled:
  RUN_CALLBACK_TEST(TestURLLoader, TrustedSameOriginRestriction, filter);
  RUN_CALLBACK_TEST(TestURLLoader, TrustedCrossOriginRequest, filter);
  RUN_CALLBACK_TEST(TestURLLoader, TrustedCorbEligibleRequest, filter);
}

std::string TestURLLoader::ReadEntireFile(pp::FileIO* file_io,
                                          std::string* data) {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  char buf[256];
  int64_t offset = 0;

  for (;;) {
    callback.WaitForResult(file_io->Read(offset, buf, sizeof(buf),
                           callback.GetCallback()));
    if (callback.result() < 0)
      return ReportError("FileIO::Read", callback.result());
    if (callback.result() == 0)
      break;
    offset += callback.result();
    data->append(buf, callback.result());
  }

  PASS();
}

std::string TestURLLoader::ReadEntireResponseBody(pp::URLLoader* loader,
                                                  std::string* body) {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  char buf[2];  // Small so that multiple reads are needed.

  for (;;) {
    callback.WaitForResult(
        loader->ReadResponseBody(buf, sizeof(buf), callback.GetCallback()));
    if (callback.result() < 0)
      return ReportError("URLLoader::ReadResponseBody", callback.result());
    if (callback.result() == 0)
      break;
    body->append(buf, callback.result());
  }

  PASS();
}

std::string TestURLLoader::LoadAndCompareBody(
    const pp::URLRequestInfo& request,
    const std::string& expected_body) {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  pp::URLLoader loader(instance_);
  callback.WaitForResult(loader.Open(request, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  pp::URLResponseInfo response_info(loader.GetResponseInfo());
  if (response_info.is_null())
    return "URLLoader::GetResponseInfo returned null";
  int32_t status_code = response_info.GetStatusCode();
  if (status_code != 200)
    return "Unexpected HTTP status code";

  std::string body;
  std::string error = ReadEntireResponseBody(&loader, &body);
  if (!error.empty())
    return error;

  if (body.size() != expected_body.size())
    return "URLLoader::ReadResponseBody returned unexpected content length";
  if (body != expected_body)
    return "URLLoader::ReadResponseBody returned unexpected content";

  PASS();
}

std::string TestURLLoader::LoadAndFail(const pp::URLRequestInfo& request) {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  pp::URLLoader loader(instance_);
  callback.WaitForResult(loader.Open(request, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_FAILED, callback.result());

  PASS();
}

int32_t TestURLLoader::OpenFileSystem(pp::FileSystem* file_system,
                                      std::string* message) {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  callback.WaitForResult(file_system->Open(1024, callback.GetCallback()));
  if (callback.failed()) {
    message->assign(callback.errors());
    return callback.result();
  }
  if (callback.result() != PP_OK) {
    message->assign("FileSystem::Open");
    return callback.result();
  }
  return callback.result();
}

int32_t TestURLLoader::PrepareFileForPost(
      const pp::FileRef& file_ref,
      const std::string& data,
      std::string* message) {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  pp::FileIO file_io(instance_);
  callback.WaitForResult(file_io.Open(file_ref,
                                      PP_FILEOPENFLAG_CREATE |
                                      PP_FILEOPENFLAG_TRUNCATE |
                                      PP_FILEOPENFLAG_WRITE,
                                      callback.GetCallback()));
  if (callback.failed()) {
    message->assign(callback.errors());
    return callback.result();
  }
  if (callback.result() != PP_OK) {
    message->assign("FileIO::Open failed.");
    return callback.result();
  }

  int32_t rv = WriteEntireBuffer(instance_->pp_instance(), &file_io, 0, data,
                                 callback_type());
  if (rv != PP_OK) {
    message->assign("FileIO::Write failed.");
    return rv;
  }

  return rv;
}

std::string TestURLLoader::GetReachableAbsoluteURL(
    const std::string& file_name) {
  // Get the absolute page URL and replace the test case file name
  // with the given one.
  pp::Var document_url(
      pp::PASS_REF,
      testing_interface_->GetDocumentURL(instance_->pp_instance(),
                                         NULL));
  std::string url(document_url.AsString());
  std::string old_name("test_case.html");
  size_t index = url.find(old_name);
  ASSERT_NE(index, std::string::npos);
  url.replace(index, old_name.length(), file_name);
  return url;
}

std::string TestURLLoader::GetReachableCrossOriginURL(
    const std::string& file_name) {
  // Get an absolute URL and use it to construct a URL that will be
  // considered cross-origin by the CORS access control code, and yet be
  // reachable by the test server.
  std::string url = GetReachableAbsoluteURL(file_name);
  // Replace '127.0.0.1' with 'localhost'.
  std::string host("127.0.0.1");
  size_t index = url.find(host);
  ASSERT_NE(index, std::string::npos);
  url.replace(index, host.length(), "localhost");
  return url;
}

int32_t TestURLLoader::OpenUntrusted(const std::string& method,
                                     const std::string& header) {
  pp::URLRequestInfo request(instance_);
  request.SetURL("/echo");
  request.SetMethod(method);
  request.SetHeaders(header);

  return OpenUntrusted(request, NULL);
}

int32_t TestURLLoader::OpenTrusted(const std::string& method,
                                   const std::string& header) {
  pp::URLRequestInfo request(instance_);
  request.SetURL("/echo");
  request.SetMethod(method);
  request.SetHeaders(header);

  return OpenTrusted(request, NULL);
}

int32_t TestURLLoader::OpenUntrusted(const pp::URLRequestInfo& request,
                                     std::string* response_body) {
  return Open(request, false, response_body);
}

int32_t TestURLLoader::OpenTrusted(const pp::URLRequestInfo& request,
                                   std::string* response_body) {
  return Open(request, true, response_body);
}

int32_t TestURLLoader::Open(const pp::URLRequestInfo& request,
                            bool trusted,
                            std::string* response_body) {
  pp::URLLoader loader(instance_);
  if (trusted)
    url_loader_trusted_interface_->GrantUniversalAccess(loader.pp_resource());

  return OpenURLRequest(instance_->pp_instance(), &loader, request,
                        callback_type(), response_body);
}

std::string TestURLLoader::TestBasicGET() {
  pp::URLRequestInfo request(instance_);
  request.SetURL("test_url_loader_data/hello.txt");
  return LoadAndCompareBody(request, "hello\n");
}

std::string TestURLLoader::TestBasicPOST() {
  pp::URLRequestInfo request(instance_);
  request.SetURL("/echo");
  request.SetMethod("POST");
  std::string postdata("postdata");
  request.AppendDataToBody(postdata.data(),
                           static_cast<uint32_t>(postdata.length()));
  return LoadAndCompareBody(request, postdata);
}

std::string TestURLLoader::TestBasicFilePOST() {
  std::string message;

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  int32_t rv = OpenFileSystem(&file_system, &message);
  if (rv != PP_OK)
    return ReportError(message.c_str(), rv);

  pp::FileRef file_ref(file_system, "/file_post_test");
  std::string postdata("postdata");
  rv = PrepareFileForPost(file_ref, postdata, &message);
  if (rv != PP_OK)
    return ReportError(message.c_str(), rv);

  pp::URLRequestInfo request(instance_);
  request.SetURL("/echo");
  request.SetMethod("POST");
  request.AppendFileToBody(file_ref, 0);
  return LoadAndCompareBody(request, postdata);
}

std::string TestURLLoader::TestBasicFileRangePOST() {
  std::string message;

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  int32_t rv = OpenFileSystem(&file_system, &message);
  if (rv != PP_OK)
    return ReportError(message.c_str(), rv);

  pp::FileRef file_ref(file_system, "/file_range_post_test");
  std::string postdata("postdatapostdata");
  rv = PrepareFileForPost(file_ref, postdata, &message);
  if (rv != PP_OK)
    return ReportError(message.c_str(), rv);

  pp::URLRequestInfo request(instance_);
  request.SetURL("/echo");
  request.SetMethod("POST");
  request.AppendFileRangeToBody(file_ref, 4, 12, 0);
  return LoadAndCompareBody(request, postdata.substr(4, 12));
}

std::string TestURLLoader::TestCompoundBodyPOST() {
  pp::URLRequestInfo request(instance_);
  request.SetURL("/echo");
  request.SetMethod("POST");
  std::string postdata1("post");
  request.AppendDataToBody(postdata1.data(),
                           static_cast<uint32_t>(postdata1.length()));
  std::string postdata2("data");
  request.AppendDataToBody(postdata2.data(),
                           static_cast<uint32_t>(postdata2.length()));
  return LoadAndCompareBody(request, postdata1 + postdata2);
}

std::string TestURLLoader::TestEmptyDataPOST() {
  pp::URLRequestInfo request(instance_);
  request.SetURL("/echo");
  request.SetMethod("POST");
  request.AppendDataToBody("", 0);
  return LoadAndCompareBody(request, std::string());
}

std::string TestURLLoader::TestBinaryDataPOST() {
  pp::URLRequestInfo request(instance_);
  request.SetURL("/echo");
  request.SetMethod("POST");
  const char postdata_chars[] =
      "\x00\x01\x02\x03\x04\x05postdata\xfa\xfb\xfc\xfd\xfe\xff";
  std::string postdata(postdata_chars,
                       sizeof(postdata_chars) / sizeof(postdata_chars[0]));
  request.AppendDataToBody(postdata.data(),
                           static_cast<uint32_t>(postdata.length()));
  return LoadAndCompareBody(request, postdata);
}

std::string TestURLLoader::TestCustomRequestHeader() {
  pp::URLRequestInfo request(instance_);
  request.SetURL("/echoheader?Foo");
  request.SetHeaders("Foo: 1");
  return LoadAndCompareBody(request, "1");
}

std::string TestURLLoader::TestFailsBogusContentLength() {
  pp::URLRequestInfo request(instance_);
  request.SetURL("/echo");
  request.SetMethod("POST");
  request.SetHeaders("Content-Length: 400");
  std::string postdata("postdata");
  request.AppendDataToBody(postdata.data(),
                           static_cast<uint32_t>(postdata.length()));

  int32_t rv;
  rv = OpenUntrusted(request, NULL);
  if (rv != PP_ERROR_NOACCESS)
    return ReportError(
        "Untrusted request with bogus Content-Length restriction", rv);

  PASS();
}

std::string TestURLLoader::TestStreamToFile() {
  pp::URLRequestInfo request(instance_);
  request.SetURL("test_url_loader_data/hello.txt");
  ASSERT_FALSE(request.SetStreamToFile(true));

  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  pp::URLLoader loader(instance_);
  callback.WaitForResult(loader.Open(request, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  pp::URLResponseInfo response_info(loader.GetResponseInfo());
  if (response_info.is_null())
    return "URLLoader::GetResponseInfo returned null";
  int32_t status_code = response_info.GetStatusCode();
  if (status_code != 200)
    return "Unexpected HTTP status code";

  pp::FileRef body(response_info.GetBodyAsFileRef());
  ASSERT_TRUE(body.is_null());

  callback.WaitForResult(loader.FinishStreamingToFile(callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_NOTSUPPORTED, callback.result());

  PASS();
}

// Untrusted, unintended cross-origin requests should fail.
std::string TestURLLoader::TestUntrustedSameOriginRestriction() {
  pp::URLRequestInfo request(instance_);
  std::string cross_origin_url = GetReachableCrossOriginURL("test_case.html");
  request.SetURL(cross_origin_url);

  int32_t rv = OpenUntrusted(request, NULL);
  if (rv != PP_ERROR_NOACCESS)
    return ReportError(
        "Untrusted, unintended cross-origin request restriction", rv);

  PASS();
}

// Trusted, unintended cross-origin requests should succeed.
// Use a CORB/ORB-passing resource, so ORB doesn't interfere with the test.
std::string TestURLLoader::TestTrustedSameOriginRestriction() {
  pp::URLRequestInfo request(instance_);
  std::string cross_origin_url = GetReachableCrossOriginURL("test_image.png");
  request.SetURL(cross_origin_url);

  int32_t rv = OpenTrusted(request, NULL);
  if (rv != PP_OK)
    return ReportError("Trusted cross-origin request failed", rv);

  PASS();
}

// Untrusted, intended cross-origin requests should use CORS and succeed.
std::string TestURLLoader::TestUntrustedCrossOriginRequest() {
  pp::URLRequestInfo request(instance_);
  std::string cross_origin_url = GetReachableCrossOriginURL("test_case.html");
  request.SetURL(cross_origin_url);
  request.SetAllowCrossOriginRequests(true);

  int32_t rv = OpenUntrusted(request, NULL);
  if (rv != PP_OK)
    return ReportError(
        "Untrusted, intended cross-origin request failed", rv);

  PASS();
}

// Trusted, intended cross-origin requests should use CORS and succeed.
// Use a CORB/ORB-passing resource, so ORB doesn't interfere with the test.
std::string TestURLLoader::TestTrustedCrossOriginRequest() {
  pp::URLRequestInfo request(instance_);
  std::string cross_origin_url = GetReachableCrossOriginURL("test_image.png");
  request.SetURL(cross_origin_url);
  request.SetAllowCrossOriginRequests(true);

  int32_t rv = OpenTrusted(request, NULL);
  if (rv != PP_OK)
    return ReportError("Trusted cross-origin request failed", rv);

  PASS();
}

// CORB (Cross-Origin Read Blocking) should apply to plugins without universal
// access.  This test is very similar to TestUntrustedSameOriginRestriction, but
// explicitly uses a CORB-eligible response (test/json + nosniff).
std::string TestURLLoader::TestUntrustedCorbEligibleRequest() {
  // It is important to use a CORB-eligible response here: text/json + nosniff.
  std::string cross_origin_url =
      GetReachableCrossOriginURL("corb_eligible_resource.json");

  pp::URLRequestInfo request(instance_);
  request.SetURL(cross_origin_url);
  request.SetAllowCrossOriginRequests(true);

  std::string response_body;
  int32_t rv = OpenUntrusted(request, &response_body);

  // Main verification - the response should be blocked.  Ideally the blocking
  // should be done before the data leaves the browser and/or network-service
  // process (the test doesn't verify this though).
  if (rv != PP_ERROR_NOACCESS) {
    return ReportError("Untrusted Javascript URL request restriction failed",
                       rv);
  }
  ASSERT_EQ("", response_body);
  PASS();
}

// CORB (Cross-Origin Read Blocking) should apply, even to plugins with
// universal access like the PDF plugin.
//
// This test is quite similar to TestTrustedSameOriginRestriction, but it
// explicitly uses a CORB-eligible response (test/json + nosniff) and also
// explicitly verifies that the response body was not blocked.
std::string TestURLLoader::TestTrustedCorbEligibleRequest() {
  // It is important to use a CORB-eligible response here: text/json + nosniff.
  std::string cross_origin_url =
      GetReachableCrossOriginURL("corb_eligible_resource.json");

  pp::URLRequestInfo request(instance_);
  request.SetURL(cross_origin_url);
  request.SetAllowCrossOriginRequests(true);

  // The test code below (similarly to the PDF plugin) sets the referrer - this
  // will propagate into network::ResourceRequest::request_initiator and should
  // match the NetworkService::AddAllowedRequestInitiatorForPlugin exemption.
  // This will pass `request_initiator_origin_lock` verification.
  std::string referrer = GetReachableAbsoluteURL("");
  request.SetCustomReferrerURL(referrer);
  request.SetHeaders(referrer);

  std::string response_body;
  int32_t rv = OpenTrusted(request, &response_body);
  // CORB + ORB "v0.1" return an empty response; ORB "v0.2" returns an error
  // code for CORB/ORB-blocked requests. This test needs to work with both.
  if (rv != PP_OK && rv != PP_ERROR_FAILED) {
    return ReportError("Trusted CORB-eligible request failed unexpectedly ",
                       rv);
  }

  // Main verification - CORB should block the response where the
  // `request_initiator` is cross-origin wrt the target URL.
  //
  // Note that this case (and CORB blocking) does never apply to the PDF plugin,
  // because the PDF plugin only triggers requests where both
  // `request_initiator` and the target URL are based on the URL of the PDF
  // document (i.e. they are same-origin wrt each other).
  ASSERT_EQ("", response_body);
  PASS();
}

// Untrusted Javascript URLs requests should fail.
std::string TestURLLoader::TestUntrustedJavascriptURLRestriction() {
  pp::URLRequestInfo request(instance_);
  request.SetURL("javascript:foo = bar");

  int32_t rv = OpenUntrusted(request, NULL);
  if (rv != PP_ERROR_NOACCESS)
    return ReportError(
        "Untrusted Javascript URL request restriction failed", rv);

  PASS();
}

// Trusted Javascript URLs requests should succeed.
std::string TestURLLoader::TestTrustedJavascriptURLRestriction() {
  pp::URLRequestInfo request(instance_);
  request.SetURL("javascript:foo = bar");

  int32_t rv = OpenTrusted(request, NULL);
  if (rv == PP_ERROR_NOACCESS)
  return ReportError(
      "Trusted Javascript URL request", rv);

  PASS();
}

std::string TestURLLoader::TestUntrustedHttpRequests() {
  // HTTP methods are restricted only for untrusted loaders. Forbidden
  // methods are CONNECT, TRACE, and TRACK, and any string that is not a
  // valid token (containing special characters like CR, LF).
  // http://www.w3.org/TR/XMLHttpRequest/
  {
    ASSERT_EQ(PP_ERROR_NOACCESS, OpenUntrusted("cOnNeCt", std::string()));
    ASSERT_EQ(PP_ERROR_NOACCESS, OpenUntrusted("tRaCk", std::string()));
    ASSERT_EQ(PP_ERROR_NOACCESS, OpenUntrusted("tRaCe", std::string()));
    ASSERT_EQ(PP_ERROR_NOACCESS,
        OpenUntrusted("POST\x0d\x0ax-csrf-token:\x20test1234", std::string()));
  }
  // HTTP methods are restricted only for untrusted loaders. Try all headers
  // that are forbidden by http://www.w3.org/TR/XMLHttpRequest/.
  {
    ASSERT_EQ(PP_ERROR_NOACCESS, OpenUntrusted("GET", "Accept-Charset:\n"));
    ASSERT_EQ(PP_ERROR_NOACCESS, OpenUntrusted("GET", "Accept-Encoding:\n"));
    ASSERT_EQ(PP_ERROR_NOACCESS, OpenUntrusted("GET", "Connection:\n"));
    ASSERT_EQ(PP_ERROR_NOACCESS, OpenUntrusted("GET", "Content-Length:\n"));
    ASSERT_EQ(PP_ERROR_NOACCESS, OpenUntrusted("GET", "Cookie:\n"));
    ASSERT_EQ(PP_ERROR_NOACCESS, OpenUntrusted("GET", "Cookie2:\n"));
    ASSERT_EQ(PP_ERROR_NOACCESS, OpenUntrusted("GET", "Date:\n"));
    ASSERT_EQ(PP_ERROR_NOACCESS, OpenUntrusted("GET", "Dnt:\n"));
    ASSERT_EQ(PP_ERROR_NOACCESS, OpenUntrusted("GET", "Expect:\n"));
    ASSERT_EQ(PP_ERROR_NOACCESS, OpenUntrusted("GET", "Host:\n"));
    ASSERT_EQ(PP_ERROR_NOACCESS, OpenUntrusted("GET", "Keep-Alive:\n"));
    ASSERT_EQ(PP_ERROR_NOACCESS, OpenUntrusted("GET", "Referer:\n"));
    ASSERT_EQ(PP_ERROR_NOACCESS, OpenUntrusted("GET", "TE:\n"));
    ASSERT_EQ(PP_ERROR_NOACCESS, OpenUntrusted("GET", "Trailer:\n"));
    ASSERT_EQ(PP_ERROR_NOACCESS,
              OpenUntrusted("GET", "Transfer-Encoding:\n"));
    ASSERT_EQ(PP_ERROR_NOACCESS, OpenUntrusted("GET", "Upgrade:\n"));
    ASSERT_EQ(PP_ERROR_NOACCESS, OpenUntrusted("GET", "User-Agent:\n"));
    ASSERT_EQ(PP_ERROR_NOACCESS, OpenUntrusted("GET", "Via:\n"));
    ASSERT_EQ(PP_ERROR_NOACCESS, OpenUntrusted(
        "GET", "Proxy-Authorization: Basic dXNlcjpwYXNzd29yZA==:\n"));
    ASSERT_EQ(PP_ERROR_NOACCESS, OpenUntrusted("GET", "Sec-foo:\n"));
  }
  // Untrusted requests with custom referrer should fail.
  {
    pp::URLRequestInfo request(instance_);
    request.SetCustomReferrerURL("http://www.google.com/");

    int32_t rv = OpenUntrusted(request, NULL);
    if (rv != PP_ERROR_NOACCESS)
      return ReportError(
          "Untrusted request with custom referrer restriction", rv);
  }
  // Untrusted requests with custom transfer encodings should fail.
  {
    pp::URLRequestInfo request(instance_);
    request.SetCustomContentTransferEncoding("foo");

    int32_t rv = OpenUntrusted(request, NULL);
    if (rv != PP_ERROR_NOACCESS)
      return ReportError(
          "Untrusted request with content-transfer-encoding restriction", rv);
  }

  PASS();
}

std::string TestURLLoader::TestTrustedHttpRequests() {
  // Trusted requests can use restricted methods, other than CONNECT, which gets
  // sockets into a problematic state.
  {
    ASSERT_EQ(PP_OK, OpenTrusted("tRaCk", std::string()));
    ASSERT_EQ(PP_OK, OpenTrusted("tRaCe", std::string()));
  }
  // Trusted requests can use restricted headers.
  {
    ASSERT_EQ(PP_OK, OpenTrusted("GET", "Accept-Charset:\n"));
    ASSERT_EQ(PP_OK, OpenTrusted("GET", "Accept-Encoding:\n"));
    ASSERT_EQ(PP_OK, OpenTrusted("GET", "Connection:\n"));
    ASSERT_EQ(PP_OK, OpenTrusted("GET", "Cookie:\n"));
    ASSERT_EQ(PP_OK, OpenTrusted("GET", "Date:\n"));
    ASSERT_EQ(PP_OK, OpenTrusted("GET", "DNT:\n"));
    ASSERT_EQ(PP_OK, OpenTrusted("GET", "Expect:\n"));

    // Host header is still forbidden because it can conflict with specific URL.

    ASSERT_EQ(PP_OK, OpenTrusted("GET", "Referer:\n"));
    ASSERT_EQ(PP_OK, OpenTrusted("GET", "User-Agent:\n"));
    ASSERT_EQ(PP_OK, OpenTrusted("GET", "Via:\n"));
    ASSERT_EQ(PP_OK, OpenTrusted("GET", "Sec-foo:\n"));
  }
  // Trusted requests with custom referrer should succeed.  Note that the
  // referrer has to be from the same origin as the plugin (this matches the
  // behavior of the PDF plugin, which after Flash removal is the only plugin
  // that depends on custom referrers).
  // Use a CORB/ORB-passing resource, so ORB doesn't interfere with the test.
  {
    pp::URLRequestInfo request(instance_);
    std::string url = GetReachableAbsoluteURL("test_image.png");
    std::string referrer = GetReachableAbsoluteURL("");
    request.SetURL(url);
    request.SetCustomReferrerURL(referrer);
    request.SetHeaders(referrer);

    int32_t rv = OpenTrusted(request, NULL);
    if (rv != PP_OK)
      return ReportError("Trusted request with custom referrer", rv);
  }
  // Trusted requests with custom transfer encodings should succeed.
  {
    pp::URLRequestInfo request(instance_);
    std::string url = GetReachableAbsoluteURL("test_image.png");
    request.SetURL(url);
    request.SetCustomContentTransferEncoding("foo");

    int32_t rv = OpenTrusted(request, NULL);
    if (rv != PP_OK)
      return ReportError(
          "Trusted request with content-transfer-encoding failed", rv);
  }

  PASS();
}

// This test should cause a redirect and ensure that the loader follows it.
std::string TestURLLoader::TestFollowURLRedirect() {
  pp::URLRequestInfo request(instance_);
  // This prefix causes the test server to return a 301 redirect.
  std::string redirect_prefix("/server-redirect?");
  // We need an absolute path for the redirect to actually work.
  std::string redirect_url =
      GetReachableAbsoluteURL("test_url_loader_data/hello.txt");
  request.SetURL(redirect_prefix.append(redirect_url));
  return LoadAndCompareBody(request, "hello\n");
}

// This test should cause a redirect and ensure that the loader runs
// the callback, rather than following the redirect.
std::string TestURLLoader::TestAuditURLRedirect() {
  pp::URLRequestInfo request(instance_);
  // This path will cause the server to return a 301 redirect.
  // This prefix causes the test server to return a 301 redirect.
  std::string redirect_prefix("/server-redirect?");
  // We need an absolute path for the redirect to actually work.
  std::string redirect_url =
      GetReachableAbsoluteURL("test_url_loader_data/hello.txt");
  request.SetURL(redirect_prefix.append(redirect_url));
  request.SetFollowRedirects(false);

  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  pp::URLLoader loader(instance_);
  callback.WaitForResult(loader.Open(request, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // Checks that the response indicates a redirect, and that the URL
  // is correct.
  pp::URLResponseInfo response_info(loader.GetResponseInfo());
  if (response_info.is_null())
    return "URLLoader::GetResponseInfo returned null";
  int32_t status_code = response_info.GetStatusCode();
  if (status_code != 301)
    return "Response status should be 301";

  // Test that the paused loader can be resumed.
  callback.WaitForResult(loader.FollowRedirect(callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());
  std::string body;
  std::string error = ReadEntireResponseBody(&loader, &body);
  if (!error.empty())
    return error;

  if (body != "hello\n")
    return "URLLoader::FollowRedirect failed";

  PASS();
}

// This test checks if the redirect restriction does not block acceptable cases
// of 307/308 GET and HEAD.
std::string TestURLLoader::TestRestrictURLRedirectCommon() {
  std::string url = GetReachableAbsoluteURL("test_url_loader_data/hello.txt");
  std::string redirect_307_prefix("/server-redirect-307?");

  {
    // Default method is GET and will follow the redirect.
    pp::URLRequestInfo request_for_default_307(instance_);
    request_for_default_307.SetURL(redirect_307_prefix.append(url));
    std::string result_for_default_307 =
        LoadAndCompareBody(request_for_default_307, "hello\n");
    if (!result_for_default_307.empty())
      return result_for_default_307;
  }

  {
    // GET will follow the redirect.
    pp::URLRequestInfo request_for_get_307(instance_);
    request_for_get_307.SetURL(redirect_307_prefix.append(url));
    request_for_get_307.SetMethod("GET");
    std::string result_for_get_307 =
        LoadAndCompareBody(request_for_get_307, "hello\n");
    if (!result_for_get_307.empty())
      return result_for_get_307;
  }

  {
    // HEAD will follow the redirect.
    pp::URLRequestInfo request_for_head_307(instance_);
    request_for_head_307.SetURL(redirect_307_prefix.append(url));
    request_for_head_307.SetMethod("HEAD");
    std::string result_for_head_307 =
        LoadAndCompareBody(request_for_head_307, "");
    if (!result_for_head_307.empty())
      return result_for_head_307;
  }

  std::string redirect_308_prefix("/server-redirect-308?");
  {
    // Default method is GET and will follow the redirect.
    pp::URLRequestInfo request_for_default_308(instance_);
    request_for_default_308.SetURL(redirect_308_prefix.append(url));
    std::string result_for_default_308 =
        LoadAndCompareBody(request_for_default_308, "hello\n");
    if (!result_for_default_308.empty())
      return result_for_default_308;
  }

  {
    // GET will follow the redirect.
    pp::URLRequestInfo request_for_get_308(instance_);
    request_for_get_308.SetURL(redirect_308_prefix.append(url));
    request_for_get_308.SetMethod("GET");
    std::string result_for_get_308 =
        LoadAndCompareBody(request_for_get_308, "hello\n");
    if (!result_for_get_308.empty())
      return result_for_get_308;
  }

  {
    // HEAD will follow the redirect.
    pp::URLRequestInfo request_for_head_308(instance_);
    request_for_head_308.SetURL(redirect_308_prefix.append(url));
    request_for_head_308.SetMethod("HEAD");
    std::string result_for_head_308 =
        LoadAndCompareBody(request_for_head_308, "");
    if (!result_for_head_308.empty())
      return result_for_head_308;
  }

  PASS();
}

// This test checks if the redirect restriction blocks the restricted cases of
// 307/308 POST.
std::string TestURLLoader::TestRestrictURLRedirectEnabled() {
  std::string url = GetReachableAbsoluteURL("test_url_loader_data/hello.txt");

  {
    // POST will be blocked and fail.
    std::string redirect_307_prefix("/server-redirect-307?");
    pp::URLRequestInfo request_for_post_307(instance_);
    request_for_post_307.SetURL(redirect_307_prefix.append(url));
    request_for_post_307.SetMethod("POST");
    std::string result_for_post_307 = LoadAndFail(request_for_post_307);
    if (!result_for_post_307.empty())
      return result_for_post_307;
  }

  {
    // POST will be blocked and fail.
    pp::URLRequestInfo request_for_post_308(instance_);
    std::string redirect_308_prefix("/server-redirect-308?");
    request_for_post_308.SetURL(redirect_308_prefix.append(url));
    request_for_post_308.SetMethod("POST");
    std::string result_for_post_308 = LoadAndFail(request_for_post_308);
    if (!result_for_post_308.empty())
      return result_for_post_308;
  }

  PASS();
}

// This test checks if the redirect restriction does not block the restricted
// cases if the restriction is disabled.
std::string TestURLLoader::TestRestrictURLRedirectDisabled() {
  std::string url = GetReachableAbsoluteURL("test_url_loader_data/hello.txt");

  {
    // POST will not be blocked, but follow the redirect.
    std::string redirect_307_prefix("/server-redirect-307?");
    pp::URLRequestInfo request_for_post_307(instance_);
    request_for_post_307.SetURL(redirect_307_prefix.append(url));
    request_for_post_307.SetMethod("POST");
    std::string result_for_post_307 =
        LoadAndCompareBody(request_for_post_307, "hello\n");
    if (!result_for_post_307.empty())
      return result_for_post_307;
  }

  {
    // POST will not be blocked, but follow the redirect.
    pp::URLRequestInfo request_for_post_308(instance_);
    std::string redirect_308_prefix("/server-redirect-308?");
    request_for_post_308.SetURL(redirect_308_prefix.append(url));
    request_for_post_308.SetMethod("POST");
    std::string result_for_post_308 =
        LoadAndCompareBody(request_for_post_308, "hello\n");
    if (!result_for_post_308.empty())
      return result_for_post_308;
  }

  PASS();
}

std::string TestURLLoader::TestAbortCalls() {
  pp::URLRequestInfo request(instance_);
  request.SetURL("test_url_loader_data/hello.txt");

  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  int32_t rv;

  // Abort |Open()|.
  {
    rv = pp::URLLoader(instance_).Open(request, callback.GetCallback());
  }
  callback.WaitForAbortResult(rv);
  CHECK_CALLBACK_BEHAVIOR(callback);

  // Abort |ReadResponseBody()|.
  {
    char buf[2] = { 0 };
    {
      pp::URLLoader loader(instance_);
      callback.WaitForResult(loader.Open(request, callback.GetCallback()));
      CHECK_CALLBACK_BEHAVIOR(callback);
      ASSERT_EQ(PP_OK, callback.result());

      rv = loader.ReadResponseBody(buf, sizeof(buf), callback.GetCallback());
    }  // Destroy |loader|.
    callback.WaitForAbortResult(rv);
    CHECK_CALLBACK_BEHAVIOR(callback);
    if (rv == PP_OK_COMPLETIONPENDING) {
      if (buf[0] || buf[1]) {
        return "URLLoader::ReadResponseBody wrote data after resource "
               "destruction.";
      }
    }
  }

  // TODO(viettrungluu): More abort tests (but add basic tests first).
  // Also test that Close() aborts properly. crbug.com/69457

  PASS();
}

std::string TestURLLoader::TestUntendedLoad() {
  pp::URLRequestInfo request(instance_);
  request.SetURL("test_url_loader_data/hello.txt");
  request.SetRecordDownloadProgress(true);
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  pp::URLLoader loader(instance_);
  callback.WaitForResult(loader.Open(request, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // We received the response callback. Yield until the network code has called
  // the loader's didReceiveData and didFinishLoading methods before we give it
  // another callback function, to make sure the loader works with no callback.
  int64_t bytes_received = 0;
  int64_t total_bytes_to_be_received = 0;
  while (true) {
    loader.GetDownloadProgress(&bytes_received, &total_bytes_to_be_received);
    if (total_bytes_to_be_received <= 0)
      return ReportError("URLLoader::GetDownloadProgress total size",
                         static_cast<int32_t>(total_bytes_to_be_received));
    if (bytes_received == total_bytes_to_be_received)
      break;
    // Yield if we're on the main thread, so that URLLoader can receive more
    // data.
    if (pp::Module::Get()->core()->IsMainThread()) {
      NestedEvent event(instance_->pp_instance());
      event.PostSignal(10);
      event.Wait();
    }
  }
  // The loader should now have the data and have finished successfully.
  std::string body;
  std::string error = ReadEntireResponseBody(&loader, &body);
  if (!error.empty())
    return error;
  if (body != "hello\n")
    return ReportError("Couldn't read data", callback.result());

  PASS();
}

int32_t TestURLLoader::OpenWithPrefetchBufferThreshold(int32_t lower,
                                                       int32_t upper) {
  pp::URLRequestInfo request(instance_);
  request.SetURL("test_url_loader_data/hello.txt");
  request.SetPrefetchBufferLowerThreshold(lower);
  request.SetPrefetchBufferUpperThreshold(upper);

  return OpenUntrusted(request, NULL);
}

std::string TestURLLoader::TestPrefetchBufferThreshold() {
  int32_t rv = OpenWithPrefetchBufferThreshold(-1, 1);
  if (rv != PP_ERROR_FAILED) {
    return ReportError("The prefetch limits contained a negative value but "
                       "the URLLoader did not fail.", rv);
  }

  rv = OpenWithPrefetchBufferThreshold(0, 1);
  if (rv != PP_OK) {
    return ReportError("The prefetch buffer limits were legal values but "
                       "the URLLoader failed.", rv);
  }

  rv = OpenWithPrefetchBufferThreshold(1000, 1);
  if (rv != PP_ERROR_FAILED) {
    return ReportError("The lower buffer value was higher than the upper but "
                       "the URLLoader did not fail.", rv);
  }

  PASS();
}

// TODO(viettrungluu): This test properly belongs elsewhere. It tests that
// Chrome properly tags URL requests made on behalf of Pepper plugins (with an
// X-Requested-With header), but this isn't, strictly speaking, a PPAPI
// behavior.
std::string TestURLLoader::TestXRequestedWithHeader() {
  pp::URLRequestInfo request(instance_);
  request.SetURL("/echoheader?X-Requested-With");
  // The name and version of the plugin is set from the command-line (see
  // chrome/test/ppapi/ppapi_test.cc.
  return LoadAndCompareBody(request, "PPAPITests/1.2.3");
}

// TODO(viettrungluu): Add tests for  Get{Upload,Download}Progress, Close
// (including abort tests if applicable).
