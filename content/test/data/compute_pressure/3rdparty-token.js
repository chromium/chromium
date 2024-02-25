// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function insert3rdPartyToken() {
  // The OT token below expires in 2033.
  // Regenerate this token with the command:
  // tools/origin_trials/generate_token.py https://example.site
  // ComputePressure_v2 --is-third-party --expire-timestamp=2000000000
  const token =
      'A4+0VfXqoVQlS8muX8d4toc46gEWlTUOq2PaeLQsY3QBrmTGSvD7KgcZGbYJ5ygZG6vaQqSrtGJkAi0JQ4UTtw4AAABzeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLnRlc3Q6NDQzIiwgImZlYXR1cmUiOiAiQ29tcHV0ZVByZXNzdXJlX3YyIiwgImV4cGlyeSI6IDIwMDAwMDAwMDAsICJpc1RoaXJkUGFydHkiOiB0cnVlfQ==';

  const tokenElement = document.createElement('meta');
  tokenElement.httpEquiv = 'origin-trial';
  tokenElement.content = token;
  document.head.appendChild(tokenElement);
}
