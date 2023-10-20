// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import static org.junit.Assert.assertThat;

import static org.chromium.base.test.util.Matchers.containsString;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.blink.mojom.PaymentCredentialInstrument;
import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.payments.mojom.PaymentCurrencyAmount;
import org.chromium.url.GURL;
import org.chromium.url.Origin;
import org.chromium.url.mojom.Url;

/** Unit tests for ClientDataJson */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ClientDataJsonTest {
    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Test
    @SmallTest
    public void testBuildClientDataJson() {
        PaymentOptions payment = new PaymentOptions();
        payment.total = new PaymentCurrencyAmount();
        payment.total.currency = "USD";
        payment.total.value = "123";
        payment.instrument = new PaymentCredentialInstrument();
        payment.instrument.displayName = "TestPay";
        payment.instrument.icon = new Url();
        payment.instrument.icon.url = "https://www.example.test/icon.png";
        payment.payeeOrigin = new org.chromium.url.internal.mojom.Origin();
        payment.payeeOrigin.scheme = "https";
        payment.payeeOrigin.host = "test.example";
        payment.payeeOrigin.port = 443;

        byte[] challenge = new byte[3];
        String relyingPartyId = "subdomain.example.test";
        String origin = "https://example.test";
        Origin topOrigin = Origin.create(new GURL("https://www.chromium.test/pay"));
        String output =
                ClientDataJson.buildClientDataJson(
                        ClientDataRequestType.PAYMENT_GET,
                        origin,
                        challenge,
                        /* isCrossOrigin= */ false,
                        payment,
                        relyingPartyId,
                        topOrigin);

        // Test that the output has the expected fields.
        assertThat(output, containsString("\"type\":\"payment.get\""));
        assertThat(output, containsString("\"challenge\":\"AAAA\""));
        assertThat(output, containsString(String.format("\"origin\":\"%s\"", origin)));
        assertThat(output, containsString("\"crossOrigin\":false"));
        assertThat(output, containsString(String.format("\"rpId\":\"%s\"", relyingPartyId)));
        // The topOrigin is formatted with no trailing slash.
        assertThat(output, containsString("\"topOrigin\":\"https://www.chromium.test\""));
        assertThat(output, containsString("\"payeeOrigin\":\"https://test.example\""));
        assertThat(output, containsString(String.format("\"value\":\"%s\"", payment.total.value)));
        assertThat(
                output,
                containsString(String.format("\"currency\":\"%s\"", payment.total.currency)));
        assertThat(
                output,
                containsString(String.format("\"icon\":\"%s\"", payment.instrument.icon.url)));
        assertThat(
                output,
                containsString(
                        String.format("\"displayName\":\"%s\"", payment.instrument.displayName)));
    }
}
