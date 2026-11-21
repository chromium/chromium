// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stands in for an older SODA binary that does not export SodaPreSandboxInit.
extern "C" __attribute__((visibility("default"))) void FakeSodaLegacyExport() {}
