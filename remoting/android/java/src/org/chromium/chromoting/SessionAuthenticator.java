// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.Activity;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;
import android.view.KeyEvent;
import android.view.View;
import android.widget.CheckBox;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AlertDialog;

import org.chromium.chromoting.jni.Client;

/**
 * This class performs the user-interaction needed to authenticate the session connection. This
 * includes showing the PIN prompt and requesting tokens for third-party authentication.
 */
public class SessionAuthenticator {
    /**
     * Application context used for getting user preferences, displaying UI, and fetching
     * third-party tokens.
     */
    private Chromoting mApplicationContext;

    /** Client connection being authenticated. */
    private final Client mClient;

    /** Provides the tokenUrlPatterns for this host during fetchThirdPartyTokens(). */
    private HostInfo mHost;

    /** Object for fetching OAuth2 access tokens from third party authorization servers. */
    private ThirdPartyTokenFetcher mTokenFetcher;

    public SessionAuthenticator(Chromoting context, Client client, HostInfo host) {
        mApplicationContext = context;
        mClient = client;
        mHost = host;
    }

    public void displayAuthenticationPrompt(boolean pairingSupported) {
        AlertDialog.Builder pinPrompt = new AlertDialog.Builder(mApplicationContext);
        pinPrompt.setTitle(mApplicationContext.getString(R.string.title_authenticate));
        pinPrompt.setMessage(mApplicationContext.getString(R.string.pin_message_android));
        pinPrompt.setIcon(android.R.drawable.ic_lock_lock);

        final View pinEntry =
                mApplicationContext.getLayoutInflater().inflate(R.layout.pin_dialog, null);
        pinPrompt.setView(pinEntry);

        final TextView pinTextView = (TextView) pinEntry.findViewById(R.id.pin_dialog_text);
        final CheckBox pinCheckBox = (CheckBox) pinEntry.findViewById(R.id.pin_dialog_check);

        if (!pairingSupported) {
            pinCheckBox.setChecked(false);
            pinCheckBox.setVisibility(View.GONE);
        }

        pinPrompt.setPositiveButton(
                R.string.connect_button, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        if (mClient.isConnected()) {
                            mClient.handleAuthenticationResponse(
                                    String.valueOf(pinTextView.getText()),
                                    pinCheckBox.isChecked(), Build.MODEL);
                        } else {
                            String message =
                                    mApplicationContext.getString(R.string.error_network_error);
                            Toast.makeText(mApplicationContext, message, Toast.LENGTH_LONG).show();
                        }
                    }
                });

        pinPrompt.setNegativeButton(
                R.string.cancel, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        mClient.destroy();
                    }
                });

        final AlertDialog pinDialog = pinPrompt.create();

        pinTextView.setOnEditorActionListener(
                new TextView.OnEditorActionListener() {
                    @Override
                    public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                        // The user pressed enter on the keypad (equivalent to the connect button).
                        pinDialog.getButton(AlertDialog.BUTTON_POSITIVE).performClick();
                        pinDialog.dismiss();
                        return true;
                    }
                });

        pinDialog.setOnCancelListener(
                new DialogInterface.OnCancelListener() {
                    @Override
                    public void onCancel(DialogInterface dialog) {
                        // The user backed out of the dialog (equivalent to the cancel button).
                        pinDialog.getButton(AlertDialog.BUTTON_NEGATIVE).performClick();
                    }
                });

        pinDialog.show();
    }

    /** Saves newly-received pairing credentials to permanent storage. */
    public void commitPairingCredentials(String host, String id, String secret) {
        // Empty |id| indicates that pairing needs to be removed.
        if (id.isEmpty()) {
            mApplicationContext.getPreferences(Activity.MODE_PRIVATE).edit()
                    .remove(host + "_id")
                    .remove(host + "_secret")
                    .apply();
        } else {
            mApplicationContext.getPreferences(Activity.MODE_PRIVATE).edit()
                    .putString(host + "_id", id)
                    .putString(host + "_secret", secret)
                    .apply();
        }
    }

    public void fetchThirdPartyToken(String tokenUrl, String clientId, String scope) {
        assert mTokenFetcher == null;

        ThirdPartyTokenFetcher.Callback callback = new ThirdPartyTokenFetcher.Callback() {
            @Override
            public void onTokenFetched(String code, String accessToken) {
                // The native client sends the OAuth authorization code to the host as the token so
                // that the host can obtain the shared secret from the third party authorization
                // server.
                String token = code;

                // The native client uses the OAuth access token as the shared secret to
                // authenticate itself with the host using spake.
                String sharedSecret = accessToken;

                mClient.onThirdPartyTokenFetched(token, sharedSecret);
            }
        };
        mTokenFetcher = new ThirdPartyTokenFetcher(mApplicationContext, mHost.getTokenUrlPatterns(),
                callback);
        mTokenFetcher.fetchToken(tokenUrl, clientId, scope);
    }

    public void onNewIntent(Intent intent) {
        if (mTokenFetcher != null) {
            if (mTokenFetcher.handleTokenFetched(intent)) {
                mTokenFetcher = null;
            }
        }
    }

    /** Returns the pairing ID for the given host, or an empty string if not set. */
    public String getPairingId(String hostId) {
        SharedPreferences prefs = mApplicationContext.getPreferences(Activity.MODE_PRIVATE);
        return prefs.getString(hostId + "_id", "");
    }

    /** Returns the pairing secret for the given host, or an empty string if not set. */
    public String getPairingSecret(String hostId) {
        SharedPreferences prefs = mApplicationContext.getPreferences(Activity.MODE_PRIVATE);
        return prefs.getString(hostId + "_secret", "");
    }
}
