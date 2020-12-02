// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let meta = document.createElement('meta');
meta.httpEquiv = 'origin-trial';

// Third party OT token with subset usage restriction. Expires in 2033.
// Generated using:
// python generate_token.py https://example.test ConversionMeasurement \
// --usage-restriction=subset --is-third-party --expire-timestamp=2000000000
meta.content = 'A8xSVOSM2Fo25Ot5f+WXIRxAVTNK+R4JLQZvX0gbwUWq6PWGggKsIi/6HvkDNmDK64/dGOo2fJwUW4Fi7NRRhQ8AAACJeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLnRlc3Q6NDQzIiwgImlzVGhpcmRQYXJ0eSI6IHRydWUsICJ1c2FnZSI6ICJzdWJzZXQiLCAiZmVhdHVyZSI6ICJDb252ZXJzaW9uTWVhc3VyZW1lbnQiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0=';
document.head.appendChild(meta);
