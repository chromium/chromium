// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.jni;

import androidx.annotation.IntDef;

import org.chromium.chromoting.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Interface used for connection state notifications. */
@SuppressWarnings("JavaLangClash")
public interface ConnectionListener {
    /**
     * This enum must match the C++ enumeration remoting::protocol::ConnectionToHost::State.
     */
    @IntDef({State.INITIALIZING, State.CONNECTING, State.AUTHENTICATED, State.CONNECTED,
            State.FAILED, State.CLOSED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface State {
        int INITIALIZING = 0;
        int CONNECTING = 1;
        int AUTHENTICATED = 2;
        int CONNECTED = 3;
        int FAILED = 4;
        int CLOSED = 5;
    }

    /**
     * This enum and {link #getErrorStringFromError} must match the C++ enumeration
     * remoting::protocol::ErrorCode.
     */
    @IntDef({Error.OK, Error.PEER_IS_OFFLINE, Error.SESSION_REJECTED, Error.INCOMPATIBLE_PROTOCOL,
            Error.AUTHENTICATION_FAILED, Error.INVALID_ACCOUNT, Error.CHANNEL_CONNECTION_ERROR,
            Error.SIGNALING_ERROR, Error.SIGNALING_TIMEOUT, Error.HOST_OVERLOAD,
            Error.MAX_SESSION_LENGTH, Error.HOST_CONFIGURATION_ERROR, Error.UNKNOWN_ERROR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Error {
        int OK = 0;
        int PEER_IS_OFFLINE = 1;
        int SESSION_REJECTED = 2;
        int INCOMPATIBLE_PROTOCOL = 3;
        int AUTHENTICATION_FAILED = 4;
        int INVALID_ACCOUNT = 5;
        int CHANNEL_CONNECTION_ERROR = 6;
        int SIGNALING_ERROR = 7;
        int SIGNALING_TIMEOUT = 8;
        int HOST_OVERLOAD = 9;
        int MAX_SESSION_LENGTH = 10;
        int HOST_CONFIGURATION_ERROR = 11;
        int UNKNOWN_ERROR = 12;
    }

    public static int getErrorStringIdFromError(@Error int error) {
        switch (error) {
            case Error.OK:
                return 0;
            case Error.PEER_IS_OFFLINE:
                return R.string.error_host_is_offline;
            case Error.SESSION_REJECTED:
                return R.string.error_invalid_access_code;
            case Error.INCOMPATIBLE_PROTOCOL:
                return R.string.error_incompatible_protocol;
            case Error.AUTHENTICATION_FAILED:
                return R.string.error_invalid_access_code;
            case Error.INVALID_ACCOUNT:
                return R.string.error_invalid_account;
            case Error.CHANNEL_CONNECTION_ERROR:
                return R.string.error_p2p_failure;
            case Error.SIGNALING_ERROR:
                return R.string.error_p2p_failure;
            case Error.SIGNALING_TIMEOUT:
                return R.string.error_host_is_offline;
            case Error.HOST_OVERLOAD:
                return R.string.error_host_overload;
            case Error.MAX_SESSION_LENGTH:
                return R.string.error_max_session_length;
            case Error.HOST_CONFIGURATION_ERROR:
                return R.string.error_host_configuration_error;
            case Error.UNKNOWN_ERROR:
                return R.string.error_unexpected;
        }
        assert false;
        return 0;
    }

    /**
     * Notified on connection state change.
     * @param state The new connection state.
     * @param error The error code, if state is Error.FAILED.
     */
    void onConnectionState(@State int state, @Error int error);
}
