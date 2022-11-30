// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_CONTENT_TEST_SUITE_BASE_H_
#define CONTENT_PUBLIC_TEST_CONTENT_TEST_SUITE_BASE_H_

#include "base/test/test_suite.h"

namespace content {
class ContentClient;

// A basis upon which test suites that use content can be built.  This suite
// initializes bits and pieces of content; see the implementation of Initialize
// for details.
class ContentTestSuiteBase : public base::TestSuite {
 public:
  ContentTestSuiteBase(const ContentTestSuiteBase&) = delete;
  ContentTestSuiteBase& operator=(const ContentTestSuiteBase&) = delete;

  // Registers content's schemes. During this call, the given content_client is
  // registered temporarily so that it can provide additional schemes.
  static void RegisterContentSchemes(ContentClient* content_client);

  // Re-initializes content's schemes even if schemes have already been
  // registered.
  static void ReRegisterContentSchemes();

  // Registers renderer/utility/gpu processes to run in-thread.
  static void RegisterInProcessThreads();

  // Initializes ResourceBundle using Content Shell's PAK file.
  static void InitializeResourceBundle();

 protected:
  ContentTestSuiteBase(int argc, char** argv);

  void Initialize() override;
};

} //  namespace content

#endif  // CONTENT_PUBLIC_TEST_CONTENT_TEST_SUITE_BASE_H_
