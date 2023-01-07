// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/testing/internals_rtc_certificate.h"

namespace blink {

bool InternalsRTCCertificate::rtcCertificateEquals(Internals& internals,
                                                   RTCCertificate* a,
                                                   RTCCertificate* b) {
  return a->Certificate() == b->Certificate();
}

}  // namespace blink
