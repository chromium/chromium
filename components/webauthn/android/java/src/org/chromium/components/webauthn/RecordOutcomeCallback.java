// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

/** Callback interface for recording a metric on the outcome of the request. */
public interface RecordOutcomeCallback {
    public void record(int resultMetricValue);
}
