// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium;

/**
 * Helper interface to report back whether the
 * payment app is ready for payment.
 */
interface IsReadyToPayServiceCallback {
    /**
     * Method to be called by the Service to indicate
     * whether the payment app is ready for payment.
     *
     * @param isReadyToPay Whether payment app is ready to pay.
     */
    oneway void handleIsReadyToPay(boolean isReadyToPay);
}
