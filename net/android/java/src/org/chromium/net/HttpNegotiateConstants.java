// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

/** Constants used by Chrome in SPNEGO authentication requests to the Android Account Manager. */
public class HttpNegotiateConstants {
    // Option bundle keys
    //
    // The token provided by in the HTTP 401 response (Base64 encoded string)
    public static final String KEY_INCOMING_AUTH_TOKEN = "incomingAuthToken";
    // The SPNEGO Context from the previous transaction (Bundle) - also used in the response bundle
    public static final String KEY_SPNEGO_CONTEXT = "spnegoContext";
    // True if delegation is allowed
    public static final String KEY_CAN_DELEGATE = "canDelegate";

    // Response bundle keys
    //
    // The returned status from the authenticator.
    public static final String KEY_SPNEGO_RESULT = "spnegoResult";

    // Name of SPNEGO feature
    public static final String SPNEGO_FEATURE = "SPNEGO";
    // Prefix of token type. Full token type is "SPNEGO:HOSTBASED:<spn>"
    public static final String SPNEGO_TOKEN_TYPE_BASE = "SPNEGO:HOSTBASED:";

    // Returned status codes
    // All OK. Returned token is valid.
    public static final int OK = 0;
    // An unexpected error. This may be caused by a programming mistake or an invalid assumption.
    public static final int ERR_UNEXPECTED = 1;
    // Request aborted due to user action.
    public static final int ERR_ABORTED = 2;
    // An unexpected, but documented, SSPI or GSSAPI status code was returned.
    public static final int ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS = 3;
    // The server's response was invalid.
    public static final int ERR_INVALID_RESPONSE = 4;
    // Credentials could not be established during HTTP Authentication.
    public static final int ERR_INVALID_AUTH_CREDENTIALS = 5;
    // An HTTP Authentication scheme was tried which is not supported on this machine.
    public static final int ERR_UNSUPPORTED_AUTH_SCHEME = 6;
    // (GSSAPI) No Kerberos credentials were available during HTTP Authentication.
    public static final int ERR_MISSING_AUTH_CREDENTIALS = 7;
    // An undocumented SSPI or GSSAPI status code was returned.
    public static final int ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS = 8;
    // The identity used for authentication is invalid.
    public static final int ERR_MALFORMED_IDENTITY = 9;
}
