// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;

import com.google.android.gms.auth.GoogleAuthException;
import com.google.android.gms.auth.GoogleAuthUtil;

import org.chromium.chromoting.base.OAuthTokenFetcher;

import java.io.IOException;

/**
 * This helper guards same auth token requesting task shouldn't be run more than once at the same
 * time.
 */
public class OAuthTokenConsumer {
    private Context mContext;
    private String mTokenScope;
    private boolean mWaitingForAuthToken;
    private String mLatestToken;

    /**
     * @param context The context used to fetch token. |context| must be an activity if user
     *                interaction is required to get back the token.
     * @param tokenScope Scope to use when fetching the OAuth token.
     */
    public OAuthTokenConsumer(Context context, String tokenScope) {
        mContext = context;
        mTokenScope = tokenScope;
        mWaitingForAuthToken = false;
    }

    /**
     * Retrieves the auth token and call the callback when it is done. callback.onTokenFetched()
     * will be called on the main thread if the retrieval succeeds, otherwise callback.onError()
     * will be called.
     * The callback will not be run if the task is already running and false will be returned in
     * that case.
     * Each OAuthTokenConsumer is supposed to work for one specific task. It is the caller's
     * responsibility to supply equivalent callbacks (variables being captured can vary) for the
     * same consumer.
     * If user recoverable exception occurs and |context| is an activity, |callback| will not be
     * run and onActivityResult() of the activity will be called instead.
     * @param account User's account name (email).
     * @param callback the callback to be called
     * @return true if the consumer will run |callback| when the token is fetched and false
     *          otherwise (meaning a previous callback is waiting to be run).
     */
    public boolean consume(String account, final OAuthTokenFetcher.Callback callback) {
        if (mWaitingForAuthToken) {
            return false;
        }
        mWaitingForAuthToken = true;

        new OAuthTokenFetcher(mContext, account, mTokenScope, new OAuthTokenFetcher.Callback() {
            @Override
            public void onTokenFetched(String token) {
                mWaitingForAuthToken = false;
                mLatestToken = token;
                callback.onTokenFetched(token);
            }

            @Override
            public void onError(@OAuthTokenFetcher.Error int error) {
                mWaitingForAuthToken = false;
                if (error != OAuthTokenFetcher.Error.INTERRUPTED) {
                    callback.onError(error);
                }
            }
        }).fetch();
        return true;
    }

    /**
     * @return The latest auth token fetched by calling consume(). This should be used right after
     *          the callback passed to consume() is run. The token may become invalid after some
     *          amount of time.
     */
    public String getLatestToken() {
        return mLatestToken;
    }

    /**
     * Revokes the latest token fetched by the consumer.
     * @param callback onTokenFetch(null) will be called on the main thread if the token is cleared
     *                 successfully. onError(error) will be called if any error occurs. |callback|
     *                 can be null, in which case token will be cleared without running the
     *                 callback.
     */
    public void revokeLatestToken(final OAuthTokenFetcher.Callback callback) {
        OAuthTokenFetcher.TASK_RUNNER.postTask(() -> {
            try {
                GoogleAuthUtil.clearToken(mContext, mLatestToken);
                mLatestToken = null;
                if (callback != null) {
                    new Handler(Looper.getMainLooper()).post(new Runnable() {
                        @Override
                        public void run() {
                            callback.onTokenFetched(null);
                        }
                    });
                }
            } catch (GoogleAuthException e) {
                if (callback != null) {
                    handleErrorOnMainThread(callback, OAuthTokenFetcher.Error.UNEXPECTED);
                    callback.onError(OAuthTokenFetcher.Error.UNEXPECTED);
                }
            } catch (IOException e) {
                if (callback != null) {
                    handleErrorOnMainThread(callback, OAuthTokenFetcher.Error.NETWORK);
                }
            }
        });
    }

    private void handleErrorOnMainThread(
            final OAuthTokenFetcher.Callback callback, final @OAuthTokenFetcher.Error int error) {
        new Handler(Looper.getMainLooper()).post(new Runnable() {
            @Override
            public void run() {
                callback.onError(error);
            }
        });
    }
}
