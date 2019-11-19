// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.content.Context;
import android.text.TextUtils;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;

import java.text.ParsePosition;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Locale;
import java.util.TimeZone;

/** Class to represent a Host returned by {@link HostListManager}. */
public class HostInfo {
    private static final String TAG = "Chromoting";

    public final String name;
    public final String id;
    public final String jabberId;
    public final String ftlId;
    public final String publicKey;
    public final boolean isOnline;
    public final String hostOfflineReason;
    public final Date updatedTime;
    public final String hostVersion;
    public final String hostOs;
    public final String hostOsVersion;

    private final ArrayList<String> mTokenUrlPatterns;

    // Format of time values coming from the Chromoting REST API.
    // This is a specific form of RFC3339 (with no timezone info).
    // Example value to be parsed: 2014-11-21T01:02:33.814Z
    private static final String RFC_3339_FORMAT = "yyyy'-'MM'-'dd'T'HH':'mm':'ss'.'SSS'Z'";

    // Value to use if time string received from the network is malformed.
    // Such malformed string should in theory never happen, but we want
    // to have a safe fallback in case it does happen.
    private static final Date FALLBACK_DATE_IN_THE_PAST = new Date(0);

    public HostInfo(String name, String id, String jabberId, String ftlId, String publicKey,
            ArrayList<String> tokenUrlPatterns, boolean isOnline, String hostOfflineReason,
            Date updatedTime, String hostVersion, String hostOs, String hostOsVersion) {
        this.name = name;
        this.id = id;
        this.jabberId = jabberId;
        this.ftlId = ftlId;
        this.publicKey = publicKey;
        this.mTokenUrlPatterns = tokenUrlPatterns;
        this.isOnline = isOnline;
        this.hostOfflineReason = hostOfflineReason;
        this.updatedTime = updatedTime;
        this.hostVersion = hostVersion;
        this.hostOs = hostOs;
        this.hostOsVersion = hostOsVersion;
    }

    private int getHostOfflineReasonResourceId(String reason) {
        switch (reason) {
            case "initialization_failed":
                return R.string.offline_reason_initialization_failed;
            case "invalid_host_configuration":
                return R.string.offline_reason_invalid_host_configuration;
            case "invalid_host_id":
                return R.string.offline_reason_invalid_host_id;
            case "invalid_oauth_credentials":
                return R.string.offline_reason_invalid_oauth_credentials;
            case "invalid_host_domain":
                return R.string.offline_reason_invalid_host_domain;
            case "login_screen_not_supported":
                return R.string.offline_reason_login_screen_not_supported;
            case "policy_read_error":
                return R.string.offline_reason_policy_read_error;
            case "policy_change_requires_restart":
                return R.string.offline_reason_policy_change_requires_restart;
            case "success_exit":
                return R.string.offline_reason_success_exit;
            case "username_mismatch":
                return R.string.offline_reason_username_mismatch;
            case "x_server_retries_exceeded":
                return R.string.offline_reason_x_server_retries_exceeded;
            case "session_retries_exceeded":
                return R.string.offline_reason_session_retries_exceeded;
            case "host_retries_exceeded":
                return R.string.offline_reason_host_retries_exceeded;
            default:
                return R.string.offline_reason_unknown;
        }
    }

    /**
     *
     * @return true if the host is incomplete, meaning the host may be newly registered and doesn't
     * have some required fields.
     */
    public boolean isIncomplete() {
        return ftlId.isEmpty() || publicKey.isEmpty();
    }

    public String getHostOfflineReasonText(Context context) {
        if (TextUtils.isEmpty(hostOfflineReason)) {
            return context.getString(R.string.host_offline_tooltip);
        }
        int resource_id =
                getHostOfflineReasonResourceId(hostOfflineReason.toLowerCase(Locale.ENGLISH));
        return resource_id == R.string.offline_reason_unknown
                ? context.getString(resource_id, hostOfflineReason)
                : context.getString(resource_id);
    }

    public ArrayList<String> getTokenUrlPatterns() {
        return new ArrayList<String>(mTokenUrlPatterns);
    }

    public static HostInfo create(JSONObject json) throws JSONException {
        assert json != null;

        ArrayList<String> tokenUrlPatterns = new ArrayList<String>();
        JSONArray jsonPatterns = json.optJSONArray("tokenUrlPatterns");

        if (jsonPatterns != null) {
            for (int i = 0; i < jsonPatterns.length(); i++) {
                String pattern = jsonPatterns.getString(i);
                if (pattern != null && !pattern.isEmpty()) {
                    tokenUrlPatterns.add(pattern);
                }
            }
        }

        final String updatedTime = json.optString("updatedTime");
        ParsePosition parsePosition = new ParsePosition(0);
        SimpleDateFormat format = new SimpleDateFormat(RFC_3339_FORMAT, Locale.US);
        format.setTimeZone(TimeZone.getTimeZone("UTC"));
        Date updatedTimeCandidate = format.parse(updatedTime, parsePosition);
        if (updatedTimeCandidate == null) {
            Log.e(TAG, "Unparseable host.updatedTime JSON: errorIndex = %d, input = %s",
                    parsePosition.getErrorIndex(), updatedTime);
            updatedTimeCandidate = FALLBACK_DATE_IN_THE_PAST;
        }
        final Date parsedUpdatedTime = updatedTimeCandidate;

        return new HostInfo(json.getString("hostName"), json.getString("hostId"),
                json.optString("jabberId"), null, json.optString("publicKey"), tokenUrlPatterns,
                json.optString("status").equals("ONLINE"), json.optString("hostOfflineReason"),
                parsedUpdatedTime, json.optString("hostVersion"), json.optString("hostOs"),
                json.optString("hostOsVersion"));
    }
}
