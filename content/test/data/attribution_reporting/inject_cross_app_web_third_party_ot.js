// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generate this token with the command:
// generate_token.py https://example.test AttributionReportingCrossAppWeb --version 3 --is-third-party --expire-timestamp=2000000000
const THIRD_PARTY_TOKEN = 'A+cJoNlsxau5JGnEnXUU0f7TmKP8mI3UpBw12FRx4uphGMl2K4BCx4wVNV7t6nHtb2LsiCyTXWMM+Lg8ub2hzQIAAACAeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLnRlc3Q6NDQzIiwgImZlYXR1cmUiOiAiQXR0cmlidXRpb25SZXBvcnRpbmdDcm9zc0FwcFdlYiIsICJleHBpcnkiOiAyMDAwMDAwMDAwLCAiaXNUaGlyZFBhcnR5IjogdHJ1ZX0=';

const tokenElement = document.createElement('meta');
tokenElement.httpEquiv = 'origin-trial';
tokenElement.content = THIRD_PARTY_TOKEN;
document.head.appendChild(tokenElement);
