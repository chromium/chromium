// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium;

import org.chromium.IsReadyToPayServiceCallback;

/**
 * Interface to determine whether a payment app
 * is ready for payment.
 */
interface IsReadyToPayService {
    /**
     * Method that will be called on the Service to query
     * whether the payment app is ready for payment.
     *
     * @param callback The callback to report back to the browser.
     */
    oneway void isReadyToPay(IsReadyToPayServiceCallback callback);
}
