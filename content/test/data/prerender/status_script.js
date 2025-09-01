// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const executed_during_prerendering = document.prerendering;

// Let the script send a fetch request, so that the test suite can verify the
// script resumes executing by checking the server's log.
navigator.sendBeacon("/activation-beacon", "");
