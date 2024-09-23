// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.webauthn.Fido2CredentialRequest.ConditionalUiState;

/**
 * This class is responsible for emitting histograms regarding CredMan usage in
 * Fido2CredentialRequest.
 */
public class CredManMetricsHelper {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        CredManCreateRequestEnum.SENT_REQUEST,
        CredManCreateRequestEnum.COULD_NOT_SEND_REQUEST,
        CredManCreateRequestEnum.SUCCESS,
        CredManCreateRequestEnum.FAILURE,
        CredManCreateRequestEnum.CANCELLED,
        CredManCreateRequestEnum.NUM_ENTRIES
    })
    public @interface CredManCreateRequestEnum {
        int SENT_REQUEST = 0;
        int COULD_NOT_SEND_REQUEST = 1;
        int SUCCESS = 2;
        int FAILURE = 3;
        int CANCELLED = 4;

        int NUM_ENTRIES = 5;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        CredManPrepareRequestEnum.SENT_REQUEST,
        CredManPrepareRequestEnum.COULD_NOT_SEND_REQUEST,
        CredManPrepareRequestEnum.SUCCESS_HAS_RESULTS,
        CredManPrepareRequestEnum.SUCCESS_NO_RESULTS,
        CredManPrepareRequestEnum.FAILURE,
        CredManPrepareRequestEnum.NUM_ENTRIES
    })
    public @interface CredManPrepareRequestEnum {
        int SENT_REQUEST = 0;
        int COULD_NOT_SEND_REQUEST = 1;
        int SUCCESS_HAS_RESULTS = 2;
        int SUCCESS_NO_RESULTS = 3;
        int FAILURE = 4;

        int NUM_ENTRIES = 5;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        CredManGetRequestEnum.SENT_REQUEST,
        CredManGetRequestEnum.COULD_NOT_SEND_REQUEST,
        CredManGetRequestEnum.SUCCESS_PASSKEY,
        CredManGetRequestEnum.SUCCESS_PASSWORD,
        CredManGetRequestEnum.FAILURE,
        CredManGetRequestEnum.CANCELLED,
        CredManGetRequestEnum.NO_CREDENTIAL_FOUND,
        CredManGetRequestEnum.NUM_ENTRIES
    })
    public @interface CredManGetRequestEnum {
        int SENT_REQUEST = 0;
        int COULD_NOT_SEND_REQUEST = 1;
        int SUCCESS_PASSKEY = 2;
        int SUCCESS_PASSWORD = 3;
        int FAILURE = 4;
        int CANCELLED = 5;
        int NO_CREDENTIAL_FOUND = 6;

        int NUM_ENTRIES = 7;
    }

    public void recordCredManCreateRequestHistogram(@CredManCreateRequestEnum int value) {
        RecordHistogram.recordEnumeratedHistogram(
                "WebAuthentication.Android.CredManCreateRequest",
                value,
                CredManCreateRequestEnum.NUM_ENTRIES);
    }

    public void recordCredmanPrepareRequestHistogram(@CredManPrepareRequestEnum int value) {
        RecordHistogram.recordEnumeratedHistogram(
                "WebAuthentication.Android.CredManPrepareRequest",
                value,
                CredManPrepareRequestEnum.NUM_ENTRIES);
    }

    public void recordCredmanPrepareRequestDuration(long durationMs, boolean credentialsFound) {
        RecordHistogram.recordTimesHistogram(
                "WebAuthentication.Android.CredManPrepareRequestDuration", durationMs);
        if (credentialsFound) {
            RecordHistogram.recordTimesHistogram(
                    "WebAuthentication.CredentialFetchDuration.CredMan", durationMs);
        }
    }

    public void reportGetCredentialMetrics(
            @CredManGetRequestEnum int value, ConditionalUiState conditionalUiState) {
        assert !(conditionalUiState == ConditionalUiState.NONE)
                        || !(value == CredManGetRequestEnum.SUCCESS_PASSWORD)
                : "Passwords cannot be received from modal requests!";
        if (conditionalUiState == ConditionalUiState.NONE) {
            RecordHistogram.recordEnumeratedHistogram(
                    "WebAuthentication.Android.CredManModalRequests",
                    value,
                    CredManGetRequestEnum.NUM_ENTRIES);
            return;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "WebAuthentication.Android.CredManConditionalRequest",
                value,
                CredManGetRequestEnum.NUM_ENTRIES);
    }
}
