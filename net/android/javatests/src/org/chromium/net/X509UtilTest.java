// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static org.chromium.net.test.util.CertTestUtil.CERTS_DIRECTORY;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.net.test.util.CertTestUtil;

import java.io.IOException;
import java.io.RandomAccessFile;
import java.security.GeneralSecurityException;
import java.util.Arrays;

/** Tests for org.chromium.net.X509Util. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class X509UtilTest {
    private static final String BAD_EKU_TEST_ROOT = "eku-test-root.pem";
    private static final String CRITICAL_CODE_SIGNING_EE = "crit-codeSigning-chain.pem";
    private static final String NON_CRITICAL_CODE_SIGNING_EE = "non-crit-codeSigning-chain.pem";
    private static final String WEB_CLIENT_AUTH_EE = "invalid_key_usage_cert.der";
    private static final String OK_CERT = "ok_cert.pem";
    private static final String GOOD_ROOT_CA = "root_ca_cert.pem";

    private static byte[] readFileBytes(String pathname) throws IOException {
        RandomAccessFile file = new RandomAccessFile(pathname, "r");
        byte[] bytes = new byte[(int) file.length()];
        int bytesRead = file.read(bytes);
        if (bytesRead != bytes.length) {
            return Arrays.copyOfRange(bytes, 0, bytesRead);
        }
        return bytes;
    }

    @After
    public void tearDown() {
        try {
            X509Util.clearTestRootCertificates();
        } catch (Exception e) {
            Assert.fail("Could not clear test root certificates: " + e.toString());
        }
    }

    @Test
    @MediumTest
    public void testEkusVerified() throws GeneralSecurityException, IOException {
        X509Util.addTestRootCertificate(CertTestUtil.pemToDer(CERTS_DIRECTORY + BAD_EKU_TEST_ROOT));
        X509Util.addTestRootCertificate(CertTestUtil.pemToDer(CERTS_DIRECTORY + GOOD_ROOT_CA));

        Assert.assertFalse(
                X509Util.verifyKeyUsage(
                        X509Util.createCertificateFromBytes(
                                CertTestUtil.pemToDer(
                                        CERTS_DIRECTORY + CRITICAL_CODE_SIGNING_EE))));

        Assert.assertFalse(
                X509Util.verifyKeyUsage(
                        X509Util.createCertificateFromBytes(
                                CertTestUtil.pemToDer(
                                        CERTS_DIRECTORY + NON_CRITICAL_CODE_SIGNING_EE))));

        Assert.assertFalse(
                X509Util.verifyKeyUsage(
                        X509Util.createCertificateFromBytes(
                                readFileBytes(CERTS_DIRECTORY + WEB_CLIENT_AUTH_EE))));

        Assert.assertTrue(
                X509Util.verifyKeyUsage(
                        X509Util.createCertificateFromBytes(
                                CertTestUtil.pemToDer(CERTS_DIRECTORY + OK_CERT))));
    }
}
