// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.Log;

import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.Signature;

import javax.crypto.Cipher;
import javax.crypto.NoSuchPaddingException;

/** Specifies all the dependencies from the native OpenSSL engine on an Android KeyStore. */
@JNINamespace("net::android")
public class AndroidKeyStore {
    private static final String TAG = "AndroidKeyStore";

    @CalledByNative
    private static String getPrivateKeyClassName(PrivateKey privateKey) {
        return privateKey.getClass().getName();
    }

    /**
     * Check if a given PrivateKey object supports a signature algorithm.
     *
     * @param privateKey The PrivateKey handle.
     * @param algorithm The signature algorithm to use.
     * @return whether the algorithm is supported.
     */
    @CalledByNative
    private static boolean privateKeySupportsSignature(PrivateKey privateKey, String algorithm) {
        try {
            Signature signature = Signature.getInstance(algorithm);
            signature.initSign(privateKey);
        } catch (NoSuchAlgorithmException | InvalidKeyException e) {
            return false;
        } catch (Exception e) {
            Log.e(TAG, "Exception while checking support for " + algorithm + ": " + e);
            return false;
        }
        return true;
    }

    /**
     * Check if a given PrivateKey object supports an encryption algorithm.
     *
     * @param privateKey The PrivateKey handle.
     * @param algorithm The signature algorithm to use.
     * @return whether the algorithm is supported.
     */
    @CalledByNative
    private static boolean privateKeySupportsCipher(PrivateKey privateKey, String algorithm) {
        try {
            Cipher cipher = Cipher.getInstance(algorithm);
            cipher.init(Cipher.ENCRYPT_MODE, privateKey);
        } catch (NoSuchAlgorithmException | NoSuchPaddingException | InvalidKeyException e) {
            return false;
        } catch (Exception e) {
            Log.e(TAG, "Exception while checking support for " + algorithm + ": " + e);
            return false;
        }
        return true;
    }

    /**
     * Sign a given message with a given PrivateKey object.
     *
     * @param privateKey The PrivateKey handle.
     * @param algorithm The signature algorithm to use.
     * @param message The message to sign.
     * @return signature as a byte buffer.
     */
    @CalledByNative
    private static byte[] signWithPrivateKey(
            PrivateKey privateKey, String algorithm, byte[] message) {
        // Hint: Algorithm names come from:
        // http://docs.oracle.com/javase/6/docs/technotes/guides/security/StandardNames.html
        Signature signature = null;
        try {
            signature = Signature.getInstance(algorithm);
        } catch (NoSuchAlgorithmException e) {
            Log.e(TAG, "Signature algorithm " + algorithm + " not supported: " + e);
            return null;
        }

        try {
            signature.initSign(privateKey);
            signature.update(message);
            return signature.sign();
        } catch (Exception e) {
            Log.e(
                    TAG,
                    "Exception while signing message with "
                            + algorithm
                            + " and "
                            + privateKey.getAlgorithm()
                            + " private key ("
                            + privateKey.getClass().getName()
                            + "): "
                            + e);
            return null;
        }
    }

    /**
     * Encrypts a given input with a given PrivateKey object.
     *
     * @param privateKey The PrivateKey handle.
     * @param algorithm The cipher to use.
     * @param message The input to encrypt.
     * @return ciphertext as a byte buffer.
     */
    @CalledByNative
    private static byte[] encryptWithPrivateKey(
            PrivateKey privateKey, String algorithm, byte[] message) {
        Cipher cipher = null;
        try {
            cipher = Cipher.getInstance(algorithm);
        } catch (NoSuchAlgorithmException | NoSuchPaddingException e) {
            Log.e(TAG, "Cipher " + algorithm + " not supported: " + e);
            return null;
        }

        try {
            cipher.init(Cipher.ENCRYPT_MODE, privateKey);
            return cipher.doFinal(message);
        } catch (Exception e) {
            Log.e(
                    TAG,
                    "Exception while encrypting input with "
                            + algorithm
                            + " and "
                            + privateKey.getAlgorithm()
                            + " private key ("
                            + privateKey.getClass().getName()
                            + "): "
                            + e);
            return null;
        }
    }
}
