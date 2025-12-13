// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.password_manager.BrowserAssistedLoginType;

/** A container for the histograms to be recorded for a WebAuthn request. */
@NullMarked
public class RequestMetrics {
    private final @Nullable @MakeCredentialOutcome Integer mMakeCredentialOutcome;
    private final @Nullable @GetAssertionOutcome Integer mGetAssertionOutcome;
    private final @Nullable @CredentialRequestResult Integer mGetAssertionResult;
    private final @Nullable @CredentialRequestResult Integer mMakeCredentialResult;
    private final @Nullable @BrowserAssistedLoginType Integer mBrowserAssistedLoginType;

    private RequestMetrics(
            @Nullable @MakeCredentialOutcome Integer makeCredentialOutcome,
            @Nullable @GetAssertionOutcome Integer getAssertionOutcome,
            @Nullable @CredentialRequestResult Integer getAssertionResult,
            @Nullable @CredentialRequestResult Integer makeCredentialResult,
            @Nullable @BrowserAssistedLoginType Integer browserAssistedLoginType) {
        mMakeCredentialOutcome = makeCredentialOutcome;
        mGetAssertionOutcome = getAssertionOutcome;
        mGetAssertionResult = getAssertionResult;
        mMakeCredentialResult = makeCredentialResult;
        mBrowserAssistedLoginType = browserAssistedLoginType;
    }

    public @Nullable @MakeCredentialOutcome Integer getMakeCredentialOutcome() {
        return mMakeCredentialOutcome;
    }

    public @Nullable @GetAssertionOutcome Integer getGetAssertionOutcome() {
        return mGetAssertionOutcome;
    }

    public @Nullable @CredentialRequestResult Integer getGetAssertionResult() {
        return mGetAssertionResult;
    }

    public @Nullable @CredentialRequestResult Integer getMakeCredentialResult() {
        return mMakeCredentialResult;
    }

    public @Nullable @BrowserAssistedLoginType Integer getBrowserAssistedLoginType() {
        return mBrowserAssistedLoginType;
    }

    public static class Builder {
        private @Nullable @MakeCredentialOutcome Integer mMakeCredentialOutcome;
        private @Nullable @GetAssertionOutcome Integer mGetAssertionOutcome;
        private @Nullable @CredentialRequestResult Integer mGetAssertionResult;
        private @Nullable @CredentialRequestResult Integer mMakeCredentialResult;
        private @Nullable @BrowserAssistedLoginType Integer mBrowserAssistedLoginType;

        public Builder setMakeCredentialOutcome(@MakeCredentialOutcome int makeCredentialOutcome) {
            mMakeCredentialOutcome = makeCredentialOutcome;
            return this;
        }

        public Builder setGetAssertionOutcome(@GetAssertionOutcome int getAssertionOutcome) {
            mGetAssertionOutcome = getAssertionOutcome;
            return this;
        }

        public Builder setGetAssertionResult(@CredentialRequestResult int getAssertionResult) {
            mGetAssertionResult = getAssertionResult;
            return this;
        }

        public Builder setMakeCredentialResult(@CredentialRequestResult int makeCredentialResult) {
            mMakeCredentialResult = makeCredentialResult;
            return this;
        }

        public Builder setBrowserAssistedLoginType(
                @BrowserAssistedLoginType Integer browserAssistedLoginType) {
            mBrowserAssistedLoginType = browserAssistedLoginType;
            return this;
        }

        public RequestMetrics build() {
            return new RequestMetrics(
                    mMakeCredentialOutcome,
                    mGetAssertionOutcome,
                    mGetAssertionResult,
                    mMakeCredentialResult,
                    mBrowserAssistedLoginType);
        }
    }
}
