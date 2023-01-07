// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.annotation.SuppressLint;
import android.content.Context;
import android.text.format.DateUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;

/** Describes the appearance and behavior of each host list entry. */
class HostListAdapter extends ArrayAdapter<HostInfo> {
    public HostListAdapter(Context context, HostInfo[] hosts) {
        super(context, -1, R.id.host_label, hosts);
    }

    /** Generates a View corresponding to this particular host. */
    @SuppressLint("ResourceType")
    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        final HostInfo host = getItem(position);
        int desiredLayoutId = host.isOnline ? R.layout.host_online : R.layout.host_offline;

        if (convertView != null && convertView.getId() != desiredLayoutId) {
            convertView = null;
        }
        if (convertView == null) {
            LayoutInflater inflater =
                    (LayoutInflater) getContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            convertView = inflater.inflate(desiredLayoutId, null);
        }

        TextView label = (TextView) convertView.findViewById(R.id.host_label);
        label.setText(host.name);

        if (!host.isOnline) {
            String offlineReasonText = host.getHostOfflineReasonText(getContext());
            CharSequence lastSeenText =
                    DateUtils.getRelativeDateTimeString(getContext(), host.updatedTime.getTime(),
                            DateUtils.SECOND_IN_MILLIS, DateUtils.WEEK_IN_MILLIS, 0);
            String statusText = getContext().getString(
                    R.string.host_status_line, lastSeenText, offlineReasonText);

            TextView status = (TextView) convertView.findViewById(R.id.host_status);
            status.setText(statusText);
        }

        return convertView;
    }
}
