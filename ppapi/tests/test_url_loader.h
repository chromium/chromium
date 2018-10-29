// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_URL_LOADER_H_
#define PPAPI_TESTS_TEST_URL_LOADER_H_

#include <stdint.h>

#include <string>

#include "ppapi/c/private/ppb_file_io_private.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/tests/test_case.h"

namespace pp {
class FileIO;
class FileRef;
class FileSystem;
class URLLoader;
class URLRequestInfo;
}

class TestURLLoader : public TestCase {
 public:
  explicit TestURLLoader(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string ReadEntireFile(pp::FileIO* file_io, std::string* data);
  std::string ReadEntireResponseBody(pp::URLLoader* loader,
                                     std::string* body);
  std::string LoadAndCompareBody(const pp::URLRequestInfo& request,
                                 const std::string& expected_body);
  int32_t OpenFileSystem(pp::FileSystem* file_system, std::string* message);
  int32_t PrepareFileForPost(const pp::FileRef& file_ref,
                             const std::string& data,
                             std::string* message);
  std::string GetReachableAbsoluteURL(const std::string& file_name);
  std::string GetReachableCrossOriginURL(const std::string& file_name);
  int32_t OpenUntrusted(const pp::URLRequestInfo& request,
                        std::string* response_body);
  int32_t OpenTrusted(const pp::URLRequestInfo& request,
                      std::string* response_body);
  int32_t OpenUntrusted(const std::string& method,
                        const std::string& header);
  int32_t OpenTrusted(const std::string& method,
                      const std::string& header);
  int32_t Open(const pp::URLRequestInfo& request,
               bool with_universal_access,
               std::string* response_body);
  int32_t OpenWithPrefetchBufferThreshold(int32_t lower, int32_t upper);

  std::string TestBasicGET();
  std::string TestBasicPOST();
  std::string TestBasicFilePOST();
  std::string TestBasicFileRangePOST();
  std::string TestCompoundBodyPOST();
  std::string TestEmptyDataPOST();
  std::string TestBinaryDataPOST();
  std::string TestCustomRequestHeader();
  std::string TestFailsBogusContentLength();
  std::string TestStreamToFile();
  std::string TestUntrustedSameOriginRestriction();
  std::string TestTrustedSameOriginRestriction();
  std::string TestUntrustedCrossOriginRequest();
  std::string TestTrustedCrossOriginRequest();
  std::string TestUntrustedCorbEligibleRequest();
  std::string TestTrustedCorbEligibleRequest();
  std::string TestUntrustedJavascriptURLRestriction();
  std::string TestTrustedJavascriptURLRestriction();
  std::string TestUntrustedHttpRequests();
  std::string TestTrustedHttpRequests();
  std::string TestFollowURLRedirect();
  std::string TestAuditURLRedirect();
  std::string TestAbortCalls();
  std::string TestUntendedLoad();
  std::string TestPrefetchBufferThreshold();
  std::string TestXRequestedWithHeader();

  const PPB_FileIO_Private* file_io_private_interface_;
  const PPB_URLLoaderTrusted* url_loader_trusted_interface_;
};

#endif  // PPAPI_TESTS_TEST_URL_LOADER_H_
