// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Base64;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

import java.security.SecureRandom;
import java.util.ArrayList;

/**
 * This class is responsible for fetching a third party token from the user using the OAuth2
 * implicit flow.  It directs the user to a third party login page located at |tokenUrl|.  It relies
 * on the |ThirdPartyTokenFetcher$OAuthRedirectActivity| to intercept the access token from the
 * redirect at intent://|REDIRECT_URI_PATH|#Intent;...end; upon successful login.
 */
public class ThirdPartyTokenFetcher {
    /** Callback for receiving the token. */
    public interface Callback {
        void onTokenFetched(String code, String accessToken);
    }

    private static final String TAG = "Chromoting";

    /** The path of the Redirect URI. */
    private static final String REDIRECT_URI_PATH = "/oauthredirect/";

    /**
     * Request both the authorization code and access token from the server.  See
     * http://tools.ietf.org/html/rfc6749#section-3.1.1.
     */
    private static final String RESPONSE_TYPE = "code token";

    /** This is used to securely generate an opaque 128 bit for the |mState| variable. */
    private static SecureRandom sSecureRandom;

    // TODO(lambroslambrou): Refactor this class to only initialize a PRNG when ThirdPartyAuth is
    // actually used.
    static {
        sSecureRandom = new SecureRandom();
    }

    /** This is used to launch the third party login page in the browser. */
    private Activity mContext;

    /**
     * An opaque value used by the client to maintain state between the request and callback.  The
     * authorization server includes this value when redirecting the user-agent back to the client.
     * The parameter is used for preventing cross-site request forgery. See
     * http://tools.ietf.org/html/rfc6749#section-10.12.
     */
    private final String mState;

    private final Callback mCallback;

    /** The list of TokenUrls allowed by the domain. */
    private final ArrayList<String> mTokenUrlPatterns;

    private final String mRedirectUriScheme;

    private final String mRedirectUri;

    public ThirdPartyTokenFetcher(Activity context,
                                  ArrayList<String> tokenUrlPatterns,
                                  Callback callback) {
        this.mContext = context;
        this.mState = generateXsrfToken();
        this.mCallback = callback;
        this.mTokenUrlPatterns = tokenUrlPatterns;

        this.mRedirectUriScheme = ContextUtils.getApplicationContext().getPackageName();

        // We don't follow the OAuth spec (http://tools.ietf.org/html/rfc6749#section-3.1.2) of the
        // redirect URI as it is possible for the other applications to intercept the redirect URI.
        // Instead, we use the intent scheme URI, which can restrict a specific package to handle
        // the intent.  See https://developer.chrome.com/multidevice/android/intents.
        this.mRedirectUri = "intent://" + REDIRECT_URI_PATH + "#Intent;"
                + "package=" + mRedirectUriScheme + ";"
                + "scheme=" + mRedirectUriScheme + ";end;";
    }

    /**
     * @param tokenUrl URL of the third party login page.
     * @param clientId The client identifier. See http://tools.ietf.org/html/rfc6749#section-2.2.
     * @param scope The scope of access request. See http://tools.ietf.org/html/rfc6749#section-3.3.
     */
    public void fetchToken(String tokenUrl, String clientId, String scope) {
        if (!isValidTokenUrl(tokenUrl)) {
            failFetchToken("Token URL does not match the domain\'s allowed URL patterns."
                    + " URL: " + tokenUrl
                    + ", patterns: " + TextUtils.join(",", this.mTokenUrlPatterns));
            return;
        }

        Uri uri = buildRequestUri(tokenUrl, clientId, scope);
        Intent intent = new Intent(Intent.ACTION_VIEW, uri);
        OAuthRedirectActivity.setEnabled(mContext, true);

        try {
            mContext.startActivity(intent);
        } catch (ActivityNotFoundException e) {
            failFetchToken("No browser is installed to open the third party authentication page.");
        }
    }

    private Uri buildRequestUri(String tokenUrl, String clientId, String scope) {
        Uri.Builder uriBuilder = Uri.parse(tokenUrl).buildUpon();
        uriBuilder.appendQueryParameter("redirect_uri", this.mRedirectUri);
        uriBuilder.appendQueryParameter("scope", scope);
        uriBuilder.appendQueryParameter("client_id", clientId);
        uriBuilder.appendQueryParameter("state", mState);
        uriBuilder.appendQueryParameter("response_type", RESPONSE_TYPE);

        return uriBuilder.build();
    }

    /** Verifies the host-supplied URL matches the domain's allowed URL patterns. */
    private boolean isValidTokenUrl(String tokenUrl) {
        for (String pattern : mTokenUrlPatterns) {
            if (tokenUrl.matches(pattern)) {
                return true;
            }
        }
        return false;
    }

    private boolean isValidIntent(Intent intent) {
        assert intent != null;

        String action = intent.getAction();

        Uri data = intent.getData();
        if (data != null) {
            return Intent.ACTION_VIEW.equals(action)
                    && this.mRedirectUriScheme.equals(data.getScheme())
                    && REDIRECT_URI_PATH.equals(data.getPath());
        }
        return false;
    }

    public boolean handleTokenFetched(Intent intent) {
        assert intent != null;

        if (!isValidIntent(intent)) {
            Log.w(TAG, "Ignoring unmatched intent.");
            return false;
        }

        String accessToken = intent.getStringExtra("access_token");
        String code = intent.getStringExtra("code");
        String state = intent.getStringExtra("state");

        if (!mState.equals(state)) {
            failFetchToken("Ignoring redirect with invalid state.");
            return false;
        }

        if (code == null || accessToken == null) {
            failFetchToken("Ignoring redirect with missing code or token.");
            return false;
        }

        mCallback.onTokenFetched(code, accessToken);
        OAuthRedirectActivity.setEnabled(mContext, false);
        return true;
    }

    private void failFetchToken(String errorMessage) {
        Log.e(TAG, "failFetchToken(): %s", errorMessage);
        mCallback.onTokenFetched("", "");
        OAuthRedirectActivity.setEnabled(mContext, false);
    }

    /** Generate a 128 bit URL-safe opaque string to prevent cross site request forgery (XSRF).*/
    private static String generateXsrfToken() {
        byte[] bytes = new byte[16];
        sSecureRandom.nextBytes(bytes);
        // Uses a variant of Base64 to make sure the URL is URL safe:
        // URL_SAFE replaces - with _ and + with /.
        // NO_WRAP removes the trailing newline character.
        // NO_PADDING removes any trailing =.
        return Base64.encodeToString(bytes, Base64.URL_SAFE | Base64.NO_WRAP | Base64.NO_PADDING);
    }

    /**
     * In the OAuth2 implicit flow, the browser will be redirected to
     * intent://|REDIRECT_URI_PATH|#Intent;...end; upon a successful login. OAuthRedirectActivity
     * uses an intent filter in the manifest to intercept the URL and launch the chromoting app.
     *
     * Unfortunately, most browsers on Android, e.g. chrome, reload the URL when a browser
     * tab is activated.  As a result, chromoting is launched unintentionally when the user restarts
     * chrome or closes other tabs that causes the redirect URL to become the topmost tab.
     *
     * To solve the problem, the redirect intent-filter is declared in a separate activity,
     * |OAuthRedirectActivity| instead of the MainActivity.  In this way, we can disable it,
     * together with its intent filter, by default. |OAuthRedirectActivity| is only enabled when
     * there is a pending token fetch request.
     */
    public static class OAuthRedirectActivity extends Activity {
        @Override
        public void onStart() {
            super.onStart();
            // |OAuthRedirectActivity| runs in its own task, it needs to route the intent back
            // to Chromoting.java to access the state of the current request.
            Intent intent = getIntent();
            intent.setClass(this, Chromoting.class);
            startActivity(intent);
            finishActivity(0);
        }

        public static void setEnabled(Activity context, boolean enabled) {
            int enabledState = enabled ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                                       : PackageManager.COMPONENT_ENABLED_STATE_DEFAULT;
            ComponentName component = new ComponentName(ContextUtils.getApplicationContext(),
                    ThirdPartyTokenFetcher.OAuthRedirectActivity.class);
            context.getPackageManager().setComponentEnabledSetting(
                    component,
                    enabledState,
                    PackageManager.DONT_KILL_APP);
        }
    }
}
