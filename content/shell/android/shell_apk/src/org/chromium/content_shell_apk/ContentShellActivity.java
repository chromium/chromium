// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell_apk;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;
import android.view.KeyEvent;
import android.widget.Toast;

import org.chromium.base.CommandLine;
import org.chromium.base.MemoryPressureListener;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.DeviceUtils;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_shell.Shell;
import org.chromium.content_shell.ShellManager;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;

/** Activity for managing the Content Shell. */
public class ContentShellActivity extends Activity {

    private static final String TAG = "ContentShellActivity";

    private static final String ACTIVE_SHELL_URL_KEY = "activeUrl";
    public static final String COMMAND_LINE_ARGS_KEY = "commandLineArgs";

    // Native switch - shell_switches::kRunWebTests
    private static final String RUN_WEB_TESTS_SWITCH = "run-web-tests";

    private ShellManager mShellManager;
    private ActivityWindowAndroid mWindowAndroid;
    private Intent mLastSentIntent;
    private String mStartupUrl;
    private IntentRequestTracker mIntentRequestTracker;

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Initializing the command line must occur before loading the library.
        if (!CommandLine.isInitialized()) {
            ((ContentShellApplication) getApplication()).initCommandLine();
            String[] commandLineParams = getCommandLineParamsFromIntent(getIntent());
            if (commandLineParams != null) {
                CommandLine.getInstance().appendSwitchesAndArguments(commandLineParams);
            }
        }

        DeviceUtils.addDeviceSpecificUserAgentSwitch();

        LibraryLoader.getInstance().ensureInitialized();

        setContentView(R.layout.content_shell_activity);
        mShellManager = findViewById(R.id.shell_container);
        final boolean listenToActivityState = true;
        mIntentRequestTracker = IntentRequestTracker.createFromActivity(this);
        mWindowAndroid =
                new ActivityWindowAndroid(this, listenToActivityState, mIntentRequestTracker);
        mIntentRequestTracker.restoreInstanceState(savedInstanceState);
        mShellManager.setWindow(mWindowAndroid);
        // Set up the animation placeholder to be the SurfaceView. This disables the
        // SurfaceView's 'hole' clipping during animations that are notified to the window.
        mWindowAndroid.setAnimationPlaceholderView(
                mShellManager.getContentViewRenderView().getSurfaceView());

        mStartupUrl = getUrlFromIntent(getIntent());

        if (CommandLine.getInstance().hasSwitch(RUN_WEB_TESTS_SWITCH)) {
            BrowserStartupController.getInstance()
                    .startBrowserProcessesSync(LibraryProcessType.PROCESS_BROWSER, false, false);
        } else {
            BrowserStartupController.getInstance()
                    .startBrowserProcessesAsync(
                            LibraryProcessType.PROCESS_BROWSER,
                            true,
                            false,
                            new BrowserStartupController.StartupCallback() {
                                @Override
                                public void onSuccess() {
                                    finishInitialization(savedInstanceState);
                                }

                                @Override
                                public void onFailure() {
                                    initializationFailed();
                                }
                            });
        }
    }

    private void finishInitialization(Bundle savedInstanceState) {
        String shellUrl;
        if (!TextUtils.isEmpty(mStartupUrl)) {
            shellUrl = mStartupUrl;
        } else {
            shellUrl = ShellManager.DEFAULT_SHELL_URL;
        }

        if (savedInstanceState != null && savedInstanceState.containsKey(ACTIVE_SHELL_URL_KEY)) {
            shellUrl = savedInstanceState.getString(ACTIVE_SHELL_URL_KEY);
        }
        mShellManager.launchShell(shellUrl);
    }

    private void initializationFailed() {
        Log.e(TAG, "ContentView initialization failed.");
        Toast.makeText(
                        ContentShellActivity.this,
                        R.string.browser_process_initialization_failed,
                        Toast.LENGTH_SHORT)
                .show();
        finish();
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        WebContents webContents = getActiveWebContents();
        if (webContents != null) {
            // TODO(yfriedman): crbug/783819 - This should use GURL serialize/deserialize.
            outState.putString(ACTIVE_SHELL_URL_KEY, webContents.getLastCommittedUrl().getSpec());
        }

        mIntentRequestTracker.saveInstanceState(outState);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            WebContents webContents = getActiveWebContents();
            if (webContents != null && webContents.getNavigationController().canGoBack()) {
                webContents.getNavigationController().goBack();
                return true;
            }
        }

        return super.onKeyUp(keyCode, event);
    }

    @Override
    protected void onNewIntent(Intent intent) {
        if (getCommandLineParamsFromIntent(intent) != null) {
            Log.i(TAG, "Ignoring command line params: can only be set when creating the activity.");
        }

        if (MemoryPressureListener.handleDebugIntent(this, intent.getAction())) return;

        String url = getUrlFromIntent(intent);
        if (!TextUtils.isEmpty(url)) {
            Shell activeView = getActiveShell();
            if (activeView != null) {
                activeView.loadUrl(url);
            }
        }
    }

    @Override
    protected void onStart() {
        super.onStart();

        WebContents webContents = getActiveWebContents();
        if (webContents != null) webContents.updateWebContentsVisibility(Visibility.VISIBLE);
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        mIntentRequestTracker.onActivityResult(requestCode, resultCode, data);
    }

    @Override
    public void startActivity(Intent i) {
        mLastSentIntent = i;
        super.startActivity(i);
    }

    @Override
    protected void onDestroy() {
        if (mShellManager != null) mShellManager.destroy();
        mWindowAndroid.destroy();
        super.onDestroy();
    }

    public Intent getLastSentIntent() {
        return mLastSentIntent;
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
    }

    private static String[] getCommandLineParamsFromIntent(Intent intent) {
        return intent != null ? intent.getStringArrayExtra(COMMAND_LINE_ARGS_KEY) : null;
    }

    /**
     * @return The {@link ShellManager} configured for the activity or null if it has not been
     *         created yet.
     */
    public ShellManager getShellManager() {
        return mShellManager;
    }

    /**
     * @return The currently visible {@link Shell} or null if one is not showing.
     */
    public Shell getActiveShell() {
        return mShellManager != null ? mShellManager.getActiveShell() : null;
    }

    /**
     * @return The {@link WebContents} owned by the currently visible {@link Shell} or null if
     *         one is not showing.
     */
    public WebContents getActiveWebContents() {
        Shell shell = getActiveShell();
        return shell != null ? shell.getWebContents() : null;
    }
}
