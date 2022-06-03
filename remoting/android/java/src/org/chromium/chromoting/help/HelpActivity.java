// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.help;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.webkit.WebResourceRequest;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;

import org.chromium.chromoting.ChromotingUtil;
import org.chromium.chromoting.R;
import org.chromium.ui.UiUtils;

/**
 * The Activity for showing the Help screen.
 */
public class HelpActivity extends AppCompatActivity {
    private static final String PLAY_STORE_URL = "market://details?id=";

    /**
     * Maximum dimension for the screenshot to be sent to the Send Feedback handler.  This size
     * ensures the size of bitmap < 1MB, which is a requirement of the handler.
     */
    private static final int MAX_FEEDBACK_SCREENSHOT_DIMENSION = 600;

    /**
     * This global variable is used for passing the screenshot from the originating Activity to the
     * HelpActivity. There seems to be no better way of doing this.
     */
    private static Bitmap sScreenshot;

    /** WebView used to display help content. */
    private WebView mWebView;

    /** Launches the Help activity. */
    public static void launch(Activity activity, String helpUrl) {
        View rootView = activity.getWindow().getDecorView().getRootView();
        sScreenshot = UiUtils.generateScaledScreenshot(rootView, MAX_FEEDBACK_SCREENSHOT_DIMENSION,
                Bitmap.Config.ARGB_8888);

        Intent intent = new Intent(activity, HelpActivity.class);
        intent.setData(Uri.parse(helpUrl));
        activity.startActivity(intent);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.help);
        mWebView = (WebView) findViewById(R.id.web_view);

        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        getSupportActionBar().setTitle(getString(R.string.actionbar_help_title));

        CharSequence appName = getTitle();
        CharSequence versionName = null;
        try {
            PackageInfo info = getPackageManager().getPackageInfo(getPackageName(), 0);
            versionName = info.versionName;
        } catch (PackageManager.NameNotFoundException ex) {
            throw new RuntimeException("Unable to get version: " + ex);
        }

        CharSequence subtitle = TextUtils.concat(appName, " ", versionName);
        getSupportActionBar().setSubtitle(subtitle);

        String initialUrl = getIntent().getDataString();
        final String initialHost = Uri.parse(initialUrl).getHost();

        mWebView.getSettings().setJavaScriptEnabled(true);
        mWebView.setWebViewClient(new WebViewClient() {
            private boolean shouldOverrideUrlLoading(final Uri uri) {
                // Make sure any links to other websites open up in an external browser.
                String host = uri.getHost();

                // Note that |host| might be null, so allow for this in the test for equality.
                if (initialHost.equals(host)) {
                    return false;
                }
                ChromotingUtil.openUrl(HelpActivity.this, uri);
                return true;
            }

            @TargetApi(Build.VERSION_CODES.N)
            @Override
            public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                return shouldOverrideUrlLoading(request.getUrl());
            }

            @SuppressWarnings("deprecation")
            @Override
            public boolean shouldOverrideUrlLoading(WebView view, String url) {
                return shouldOverrideUrlLoading(Uri.parse(url));
            }
        });
        mWebView.loadUrl(initialUrl);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.help_actionbar, menu);
        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (id == android.R.id.home) {
            finish();
            return true;
        }
        if (id == R.id.actionbar_feedback) {
            FeedbackSender.sendFeedback(this, sScreenshot);
            return true;
        }
        if (id == R.id.actionbar_play_store) {
            ChromotingUtil.openUrl(this, Uri.parse(PLAY_STORE_URL + getPackageName()));
            return true;
        }
        if (id == R.id.actionbar_credits) {
            startActivity(new Intent(this, CreditsActivity.class));
            return true;
        }
        return super.onOptionsItemSelected(item);
    }
}
