// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let meta = document.createElement('meta');
meta.httpEquiv = 'origin-trial';

// Third party OT token with subset usage restriction. Expires in 2033.
// Generated using:
// python generate_token.py https://example.test AttributionReporting \
// --usage-restriction=subset --is-third-party --expire-timestamp=2000000000
meta.content = 'AwcYHeC1CwkNTksEvdIHHIDSuz+xzNsrkeDgg+6zlRWO8sljNr19o/rzFp2cLekz2arxAdag8AYK7e8yMynF4woAAACIeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLnRlc3Q6NDQzIiwgImZlYXR1cmUiOiAiQXR0cmlidXRpb25SZXBvcnRpbmciLCAiZXhwaXJ5IjogMjAwMDAwMDAwMCwgImlzVGhpcmRQYXJ0eSI6IHRydWUsICJ1c2FnZSI6ICJzdWJzZXQifQ==';
document.head.appendChild(meta);
