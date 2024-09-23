// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/scoped_ignore_content_verifier_for_test.h"

#include "extensions/browser/content_verifier/content_verify_job.h"

namespace extensions {

ScopedIgnoreContentVerifierForTest::ScopedIgnoreContentVerifierForTest() {
  ContentVerifyJob::SetIgnoreVerificationForTests(true);
}

ScopedIgnoreContentVerifierForTest::~ScopedIgnoreContentVerifierForTest() {
  ContentVerifyJob::SetIgnoreVerificationForTests(false);
}

}  // namespace extensions
