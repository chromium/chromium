// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.annotation.SuppressLint;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.support.v7.app.ActionBar.OnMenuVisibilityListener;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.util.DisplayMetrics;
import android.view.DisplayCutout;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.View.OnTouchListener;
import android.view.WindowManager;
import android.view.WindowManager.LayoutParams;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.view.inputmethod.InputMethodManager;

import androidx.annotation.IntDef;

import org.chromium.chromoting.help.HelpContext;
import org.chromium.chromoting.help.HelpSingleton;
import org.chromium.chromoting.jni.Client;
import org.chromium.ui.KeyboardVisibilityDelegate;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * A simple screen that does nothing except display a DesktopView and notify it of rotations.
 */
public class Desktop
        extends AppCompatActivity implements View.OnSystemUiVisibilityChangeListener,
                                             CapabilityManager.CapabilitiesChangedListener {
    /** Used to set/store the selected input mode. */
    @IntDef({InputMode.UNKNOWN, InputMode.TRACKPAD, InputMode.TOUCH})
    @Retention(RetentionPolicy.SOURCE)
    public @interface InputMode {
        // Values are starting from 0 and don't have gaps.
        int UNKNOWN = 0;
        int TRACKPAD = 1;
        int TOUCH = 2;
        int NUM_ENTRIES = 3;
    }

    public static String getInputModeName(@InputMode int mode) {
        switch (mode) {
            case InputMode.UNKNOWN:
                return "UNKNOWN";
            case InputMode.TRACKPAD:
                return "TRACKPAD";
            case InputMode.TOUCH:
                return "TOUCH";
        }
        assert false : "Unreached";
        return "";
    }

    /** Preference used to track the last input mode selected by the user. */
    private static final String PREFERENCE_INPUT_MODE = "input_mode";
    private static final String PREFERENCE_RESIZE_TO_CLIENT = "resize_to_client";

    /** The amount of time to wait to hide the ActionBar after user input is seen. */
    private static final int ACTIONBAR_AUTO_HIDE_DELAY_MS = 3000;

    /** Duration for fade-in and fade-out animations for the ActionBar. */
    private static final int ACTIONBAR_ANIMATION_DURATION_MS = 250;

    private final Event
            .Raisable<SystemUiVisibilityChangedEventParameter> mOnSystemUiVisibilityChanged =
            new Event.Raisable<>();

    private final Event.Raisable<InputModeChangedEventParameter> mOnInputModeChanged =
            new Event.Raisable<>();

    private Client mClient;
    private InputEventSender mInjector;

    private ActivityLifecycleListener mActivityLifecycleListener;

    /** Indicates whether a Soft Input UI (such as a keyboard) is visible. */
    private boolean mSoftInputVisible;

    /** Indicates whether resize-to-client is enabled. */
    private boolean mResizeToClientEnabled;

    /** Holds the scheduled task object which will be called to hide the ActionBar. */
    private Runnable mActionBarAutoHideTask;

    /** The Toolbar instance backing our SupportActionBar. */
    private Toolbar mToolbar;

    /** Tracks the current input mode (e.g. trackpad/touch). */
    private @InputMode int mInputMode = InputMode.UNKNOWN;

    /** Indicates whether the remote host supports touch injection. */
    private @CapabilityManager.HostCapability int mHostTouchCapability =
            CapabilityManager.HostCapability.UNKNOWN;

    private DesktopView mRemoteHostDesktop;

    /**
     * Indicates whether the device is connected to a non-hidden physical qwerty keyboard. This
     * is set by {@link Desktop#setKeyboardState(Configuration)}. DO NOT request a soft keyboard
     * when a physical keyboard exists, otherwise the activity will enter an undefined state
     * where the soft keyboard never shows up meanwhile request to hide status bar always fails.
     */
    private boolean mHasPhysicalKeyboard;

    /** Tracks whether the activity is in windowed mode. */
    private boolean mIsInWindowedMode;

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.desktop);

        mClient = Client.getInstance();
        mInjector = new InputEventSender(mClient);

        Preconditions.notNull(mClient);

        mToolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(mToolbar);

        mRemoteHostDesktop = (DesktopView) findViewById(R.id.desktop_view);
        mRemoteHostDesktop.init(mClient, this, mClient.getRenderStub());

        getSupportActionBar().setDisplayShowTitleEnabled(false);
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);

        // For this Activity, the home button in the action bar acts as a Disconnect button, so
        // set the description for accessibility/screen readers.
        getSupportActionBar().setHomeActionContentDescription(R.string.disconnect_myself_button);

        // The action bar is already shown when the activity is started however calling the
        // function below will set our preferred system UI flags which will adjust the layout
        // size of the canvas and we can avoid an initial resize event.
        showSystemUi();

        View decorView = getWindow().getDecorView();
        decorView.setOnSystemUiVisibilityChangeListener(this);

        // The background color is displayed when the user resizes the window in split-screen
        // past the boundaries of the image we render.  The default background is white and we
        // use black for our canvas, thus there is a visual artifact when we draw the canvas
        // over the background.  Setting the background color to match our canvas will prevent
        // the flash.
        getWindow().setBackgroundDrawable(new ColorDrawable(Color.BLACK));

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            // Short edges mode makes DesktopView stretch to the whole screen even if it gets
            // obstructed by the cutouts.
            WindowManager.LayoutParams layoutParams = getWindow().getAttributes();
            layoutParams.layoutInDisplayCutoutMode =
                    LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(layoutParams);
        }

        mActivityLifecycleListener = mClient.getCapabilityManager().onActivityAcceptingListener(
                this, Capabilities.CAST_CAPABILITY);
        mActivityLifecycleListener.onActivityCreated(this, savedInstanceState);

        mInputMode = getInitialInputModeValue();
        mResizeToClientEnabled = getStoredResizeToClientEnabled();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            attachSystemUiResizeListener();

            // Suspend the ActionBar timer when the user interacts with the options menu.
            getSupportActionBar().addOnMenuVisibilityListener(new OnMenuVisibilityListener() {
                @Override
                public void onMenuVisibilityChanged(boolean isVisible) {
                    if (isVisible) {
                        stopActionBarAutoHideTimer();
                    } else {
                        startActionBarAutoHideTimer();
                    }
                }
            });
        } else {
            mRemoteHostDesktop.setFitsSystemWindows(true);
        }
    }

    @Override
    protected void onStart() {
        super.onStart();
        mActivityLifecycleListener.onActivityStarted(this);
        mClient.enableVideoChannel(true);
        mClient.getCapabilityManager().addListener(this);
    }

    @Override
    public void onResume() {
        super.onResume();
        mActivityLifecycleListener.onActivityResumed(this);
        mClient.enableVideoChannel(true);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            // We want to call the change handler with an initial value as
            // onMultiWindowModeChanged won't be called if the state hasn't changed, such as
            // when the user resizes in split-screen, and we want to ensure we have a default
            // value set (even though it may change soon after).
            onMultiWindowModeChanged(isInMultiWindowMode());
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            setUpAutoHideToolbar();
            syncActionBarToSystemUiState();
        }
    }

    @Override
    protected void onPause() {
        if (isFinishing()) {
            mActivityLifecycleListener.onActivityPaused(this);
        }
        super.onPause();
        // The activity is paused in windowed mode when the user switches to another window.  In
        // that case we should leave the video channel running so they continue to see updates
        // from their remote machine.  The video channel will be stopped when onStop() is
        // called.
        if (!mIsInWindowedMode) {
            mClient.enableVideoChannel(false);
        }
        stopActionBarAutoHideTimer();
    }

    @Override
    protected void onStop() {
        mClient.getCapabilityManager().removeListener(this);
        mActivityLifecycleListener.onActivityStopped(this);
        super.onStop();
        mClient.enableVideoChannel(false);
    }

    @Override
    protected void onDestroy() {
        mRemoteHostDesktop.destroy();
        super.onDestroy();
    }

    @Override
    public void onMultiWindowModeChanged(boolean isInMultiWindowMode) {
        super.onMultiWindowModeChanged(isInMultiWindowMode);

        mIsInWindowedMode = isInMultiWindowMode;
        if (!mIsInWindowedMode) {
            setUpAutoHideToolbar();
            syncActionBarToSystemUiState();
        } else if (mActionBarAutoHideTask != null) {
            stopActionBarAutoHideTimer();
            mActionBarAutoHideTask = null;
            showSystemUi();
            syncActionBarToSystemUiState();
        }
    }

    /** Called to initialize the action bar. */
    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.desktop_actionbar, menu);

        mActivityLifecycleListener.onActivityCreatedOptionsMenu(this, menu);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            // We don't need to show a hide ActionBar button if immersive fullscreen is
            // supported.
            menu.findItem(R.id.actionbar_hide).setVisible(false);

            // Although the MenuItems are being created here, they do not have any backing Views
            // yet as those are created just after this method exits.  We post an async task to
            // the UI thread here so that we can attach our interaction listeners shortly after
            // the views have been created.
            final Menu menuFinal = menu;
            new Handler().post(new Runnable() {
                @Override
                public void run() {
                    // Attach a listener to the toolbar itself then attach one to each menu item
                    // which has a backing view object.
                    attachToolbarInteractionListenerToView(mToolbar);
                    int items = menuFinal.size();
                    for (int i = 0; i < items; i++) {
                        int itemId = menuFinal.getItem(i).getItemId();
                        View menuItemView = findViewById(itemId);
                        if (menuItemView != null) {
                            attachToolbarInteractionListenerToView(menuItemView);
                        }
                    }
                }
            });
        }

        ChromotingUtil.tintMenuIcons(this, menu);

        // Wait to set the input mode until after the default tinting has been applied.
        setInputMode(mInputMode);

        setResizeToClientEnabled(mResizeToClientEnabled);

        // Keyboard state must be set after the keyboard icon has been added to the menu.
        setKeyboardState(getResources().getConfiguration());

        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public void onConfigurationChanged(Configuration config) {
        super.onConfigurationChanged(config);
        setKeyboardState(config);
    }

    private void setKeyboardState(Configuration configuration) {
        mHasPhysicalKeyboard = (configuration.keyboard == Configuration.KEYBOARD_QWERTY)
                && (configuration.hardKeyboardHidden == Configuration.HARDKEYBOARDHIDDEN_NO);
        mToolbar.getMenu().findItem(R.id.actionbar_keyboard).setVisible(!mHasPhysicalKeyboard);
    }

    public Event<SystemUiVisibilityChangedEventParameter> onSystemUiVisibilityChanged() {
        return mOnSystemUiVisibilityChanged;
    }

    public Event<InputModeChangedEventParameter> onInputModeChanged() {
        return mOnInputModeChanged;
    }

    private @InputMode int getInitialInputModeValue() {
        // Load the previously-selected input mode from Preferences.
        // TODO(joedow): Evaluate and determine if we should use a different input mode based on
        //               a device characteristic such as screen size.
        @InputMode
        int defaultInputMode = InputMode.TRACKPAD;
        String previousInputMode =
                getPreferences(MODE_PRIVATE)
                        .getString(PREFERENCE_INPUT_MODE, getInputModeName(defaultInputMode));

        for (int i = 0; i < InputMode.NUM_ENTRIES; i++) {
            if (getInputModeName(i).equals(previousInputMode)) return i;
        }

        // Invalid or unexpected value was found, just use the default mode.
        return defaultInputMode;
    }

    private void setInputMode(@InputMode int inputMode) {
        Menu menu = mToolbar.getMenu();
        MenuItem trackpadModeMenuItem = menu.findItem(R.id.actionbar_trackpad_mode);
        MenuItem touchModeMenuItem = menu.findItem(R.id.actionbar_touch_mode);
        if (inputMode == InputMode.TRACKPAD) {
            trackpadModeMenuItem.setVisible(true);
            touchModeMenuItem.setVisible(false);
        } else if (inputMode == InputMode.TOUCH) {
            touchModeMenuItem.setVisible(true);
            trackpadModeMenuItem.setVisible(false);
        } else {
            assert false : "Unreached";
            return;
        }

        mInputMode = inputMode;
        getPreferences(MODE_PRIVATE)
                .edit()
                .putString(PREFERENCE_INPUT_MODE, getInputModeName(mInputMode))
                .apply();

        mOnInputModeChanged.raise(
                new InputModeChangedEventParameter(mInputMode, mHostTouchCapability));
    }

    private boolean getStoredResizeToClientEnabled() {
        return getPreferences(MODE_PRIVATE).getBoolean(PREFERENCE_RESIZE_TO_CLIENT, false);
    }

    private void setResizeToClientEnabled(boolean resizeToClientEnabled) {
        mResizeToClientEnabled = resizeToClientEnabled;
        Menu menu = mToolbar.getMenu();
        MenuItem resizeToClientMenuItem = menu.findItem(R.id.resize_to_client);
        resizeToClientMenuItem.setChecked(mResizeToClientEnabled);
        if (mResizeToClientEnabled != getStoredResizeToClientEnabled()) {
            getPreferences(MODE_PRIVATE)
                    .edit()
                    .putBoolean(PREFERENCE_RESIZE_TO_CLIENT, mResizeToClientEnabled)
                    .apply();
        }
        if (mResizeToClientEnabled) {
            sendPreferredHostResolution();
        }
    }

    @Override
    public void onCapabilitiesChanged(List<String> newCapabilities) {
        if (newCapabilities.contains(Capabilities.TOUCH_CAPABILITY)) {
            mHostTouchCapability = CapabilityManager.HostCapability.SUPPORTED;
        } else {
            mHostTouchCapability = CapabilityManager.HostCapability.UNSUPPORTED;
        }

        mOnInputModeChanged.raise(
                new InputModeChangedEventParameter(mInputMode, mHostTouchCapability));
    }

    // Any time an onTouchListener is attached, a lint warning about filtering touch events is
    // generated.  Since the function below is only used to listen to, not intercept, the
    // events, the lint warning can be safely suppressed.
    @SuppressLint("ClickableViewAccessibility")
    private void attachToolbarInteractionListenerToView(View view) {
        view.setOnTouchListener(new OnTouchListener() {
            @Override
            public boolean onTouch(View view, MotionEvent event) {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        stopActionBarAutoHideTimer();
                        break;

                    case MotionEvent.ACTION_UP:
                        startActionBarAutoHideTimer();
                        break;

                    default:
                        // Ignore.
                        break;
                }

                return false;
            }
        });
    }

    private void setUpAutoHideToolbar() {
        if (mActionBarAutoHideTask != null) {
            return;
        }

        mActionBarAutoHideTask = new Runnable() {
            @Override
            public void run() {
                if (!mToolbar.isOverflowMenuShowing()) {
                    hideSystemUi();
                }
            }
        };
    }

    // Posts a deplayed task to hide the ActionBar.  If an existing task has already been
    // scheduled, then the previous task is removed and the new one scheduled, effectively
    // resetting the timer.
    private void startActionBarAutoHideTimer() {
        if (mActionBarAutoHideTask != null) {
            stopActionBarAutoHideTimer();
            getWindow().getDecorView().postDelayed(
                    mActionBarAutoHideTask, ACTIONBAR_AUTO_HIDE_DELAY_MS);
        }
    }

    // Clear all existing delayed tasks to prevent the ActionBar from being hidden.
    private void stopActionBarAutoHideTimer() {
        if (mActionBarAutoHideTask != null) {
            getWindow().getDecorView().removeCallbacks(mActionBarAutoHideTask);
        }
    }

    // Updates the ActionBar visibility to match the System UI elements.  This is useful after a
    // power or activity lifecycle event in which the current System UI state has changed but we
    // never received the notification.
    private void syncActionBarToSystemUiState() {
        onSystemUiVisibilityChange(getWindow().getDecorView().getSystemUiVisibility());
    }

    private boolean isActionBarVisible() {
        return getSupportActionBar() != null && getSupportActionBar().isShowing();
    }

    private boolean isSystemUiVisible() {
        return (getWindow().getDecorView().getSystemUiVisibility() & getFullscreenFlags()) == 0;
    }

    /** Called whenever the visibility of the system status bar or navigation bar changes. */
    @Override
    public void onSystemUiVisibilityChange(int visibility) {
        // Ensure the action-bar's visibility matches that of the system controls. This
        // minimizes the number of states the UI can be in, to keep things simple for the user.

        // Check if the system is in fullscreen/lights-out mode then update the ActionBar to
        // match.
        int fullscreenFlags = getFullscreenFlags();
        if ((visibility & fullscreenFlags) != 0) {
            hideActionBar();
        } else {
            showActionBar();
        }
    }

    @SuppressLint("InlinedApi")
    private static int getFullscreenFlags() {
        // LOW_PROFILE gives the status and navigation bars a "lights-out" appearance.
        // FULLSCREEN hides the status bar on supported devices (4.1 and above).
        int flags = View.SYSTEM_UI_FLAG_LOW_PROFILE;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
            flags |= View.SYSTEM_UI_FLAG_FULLSCREEN;
        }
        return flags;
    }

    @SuppressLint("InlinedApi")
    private static int getLayoutFlags() {
        int flags = 0;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            flags |= View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
            flags |= View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
            flags |= View.SYSTEM_UI_FLAG_LAYOUT_STABLE;
        }
        return flags;
    }

    /**
     * @return The insets from each edge on the screen that avoid display cutouts.
     */
    @SuppressLint("InlinedApi")
    public Rect getSafeInsets() {
        Rect insets = new Rect();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            return insets;
        }

        DisplayCutout cutout = mRemoteHostDesktop.getRootWindowInsets().getDisplayCutout();
        if (cutout != null) {
            insets.set(cutout.getSafeInsetLeft(), cutout.getSafeInsetTop(),
                    cutout.getSafeInsetRight(), cutout.getSafeInsetBottom());
        }
        return insets;
    }

    /**
     * Shows the soft keyboard if no physical keyboard is attached.
     */
    public void showKeyboard() {
        if (!mHasPhysicalKeyboard) {
            KeyboardVisibilityDelegate.getInstance().showKeyboard(mRemoteHostDesktop);
        }
    }

    public void showSystemUi() {
        // Request exit from any fullscreen mode. The action-bar controls will be shown in
        // response to the SystemUiVisibility notification. The visibility of the action-bar
        // should be tied to the fullscreen state of the system, so there's no need to
        // explicitly show it here.
        int flags = View.SYSTEM_UI_FLAG_VISIBLE | getLayoutFlags();
        getWindow().getDecorView().setSystemUiVisibility(flags);

        // The OS will not call onSystemUiVisibilityChange() if the soft keyboard is visible
        // which means our ActionBar will not be shown if this function is called in that
        // scenario.
        if (mSoftInputVisible) {
            showActionBar();
        }
    }

    /** Shows the action bar without changing SystemUiVisibility. */
    private void showActionBar() {
        Animation animation = AnimationUtils.loadAnimation(this, android.R.anim.fade_in);
        animation.setDuration(ACTIONBAR_ANIMATION_DURATION_MS);
        mToolbar.startAnimation(animation);

        getSupportActionBar().show();
        startActionBarAutoHideTimer();
    }

    @SuppressLint("InlinedApi")
    public void hideSystemUi() {
        // If a soft input device is present, then hide the ActionBar but do not hide the rest
        // of system UI.  A second call will be made once the soft input device is hidden.
        if (mSoftInputVisible) {
            hideActionBar();
            return;
        }

        // Request the device to enter fullscreen mode. Don't hide the controls yet, because the
        // system might not honor the fullscreen request immediately (for example, if the
        // keyboard is visible, the system might delay fullscreen until the keyboard is hidden).
        // The controls will be hidden in response to the SystemUiVisibility notification.
        // This helps ensure that the visibility of the controls is synchronized with the
        // fullscreen state.
        int flags = getFullscreenFlags();

        // HIDE_NAVIGATION hides the navigation bar. However, if the user touches the screen,
        // the event is not seen by the application and instead the navigation bar is re-shown.
        // IMMERSIVE fixes this problem and allows the user to interact with the app while
        // keeping the navigation controls hidden. This flag was introduced in 4.4, later than
        // HIDE_NAVIGATION, and so a runtime check is needed before setting either of these
        // flags.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            flags |= View.SYSTEM_UI_FLAG_HIDE_NAVIGATION;
            flags |= View.SYSTEM_UI_FLAG_IMMERSIVE;
        }
        flags |= getLayoutFlags();

        getWindow().getDecorView().setSystemUiVisibility(flags);
    }

    /** Hides the action bar without changing SystemUiVisibility. */
    private void hideActionBar() {
        Animation animation = AnimationUtils.loadAnimation(this, android.R.anim.fade_out);
        animation.setDuration(ACTIONBAR_ANIMATION_DURATION_MS);
        mToolbar.startAnimation(animation);

        getSupportActionBar().hide();
        stopActionBarAutoHideTimer();
    }

    /** Called whenever an action bar button is pressed. */
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();

        mActivityLifecycleListener.onActivityOptionsItemSelected(this, item);

        if (id == R.id.actionbar_trackpad_mode) {
            // When the trackpad icon is tapped, we want to switch the input mode to touch.
            setInputMode(InputMode.TOUCH);
            return true;
        }
        if (id == R.id.actionbar_touch_mode) {
            // When the touch icon is tapped, we want to switch the input mode to trackpad.
            setInputMode(InputMode.TRACKPAD);
            return true;
        }
        if (id == R.id.actionbar_keyboard) {
            ((InputMethodManager) getSystemService(INPUT_METHOD_SERVICE)).toggleSoftInput(0, 0);
            return true;
        }
        if (id == R.id.actionbar_hide) {
            hideSystemUi();
            return true;
        }
        if (id == R.id.actionbar_disconnect || id == android.R.id.home) {
            mClient.destroy();
            return true;
        }
        if (id == R.id.actionbar_send_ctrl_alt_del) {
            mInjector.sendCtrlAltDel();
            return true;
        }
        if (id == R.id.resize_to_client) {
            setResizeToClientEnabled(!item.isChecked());
        }
        if (id == R.id.actionbar_help) {
            HelpSingleton.getInstance().launchHelp(this, HelpContext.DESKTOP);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void attachSystemUiResizeListener() {
        View systemUiResizeDetector = findViewById(R.id.resize_detector);
        systemUiResizeDetector.addOnLayoutChangeListener(new OnLayoutChangeListener() {
            // Tracks the maximum 'bottom' value seen during layout changes.  This value
            // represents the top of the SystemUI displayed at the bottom of the screen. Note:
            // This value is a screen coordinate so a larger value means lower on the screen.
            private int mMaxBottomValue;

            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                // As the activity is started, a number of layout changes will flow through.  If
                // this is a fresh start, then we will see one layout change which will
                // represent the steady state of the UI and will include an accurate 'bottom'
                // value.  If we are transitioning from another activity/orientation, then there
                // may be several layout change events as the view is updated (i.e. the OSK
                // might have been displayed previously but is being dismissed).  Therefore we
                // want to track the largest value we have seen and use it to determine if a new
                // system UI (such as the OSK) is being displayed.
                if (mMaxBottomValue < bottom) {
                    mMaxBottomValue = bottom;
                    return;
                }

                // If the delta between lowest bound we have seen (should be a System UI such as
                // the navigation bar) and the current bound does not match, then we have a form
                // of soft input displayed.  Note that the size of a soft input device can
                // change when the input method is changed so we want to send updates to the
                // image canvas whenever they occur.
                boolean oldSoftInputVisible = mSoftInputVisible;
                mSoftInputVisible = (bottom < mMaxBottomValue);

                // Send the System UI sizes if either the Soft Keyboard is displayed or if we
                // are in windowed mode and there is System UI present.  The user needs to be
                // able to move the canvas so they can see where they are typing in the first
                // case and in the second, the System UI is always present so the user needs a
                // way to position the canvas so all parts of the desktop can be made visible.
                if (mSoftInputVisible || (mIsInWindowedMode && isSystemUiVisible())) {
                    mOnSystemUiVisibilityChanged.raise(
                            new SystemUiVisibilityChangedEventParameter(left, top, right, bottom));
                } else {
                    mOnSystemUiVisibilityChanged.raise(
                            new SystemUiVisibilityChangedEventParameter(0, 0, 0, 0));
                }

                boolean softInputVisibilityChanged = oldSoftInputVisible != mSoftInputVisible;
                if (!mSoftInputVisible && softInputVisibilityChanged && !isActionBarVisible()) {
                    // Queue a task which will run after the current action (OSK dismiss) has
                    // completed, otherwise the hide request will not take effect.
                    new Handler().post(new Runnable() {
                        @Override
                        public void run() {
                            if (!mSoftInputVisible && !isActionBarVisible()) {
                                hideSystemUi();
                            }
                        }
                    });
                }
            }
        });
    }

    /**
     * Sends preferred resolution to the host that matches the aspect ratio of the screen. This
     * is calculated by the size of the DesktopView minus the safe insets. This method does
     * nothing if resize-to-client has not been enabled by the user.
     */
    public void sendPreferredHostResolution() {
        if (!mResizeToClientEnabled) {
            return;
        }

        Rect safeInsets = getSafeInsets();
        int safeAreaWidth = mRemoteHostDesktop.getWidth() - safeInsets.left - safeInsets.right;
        int safeAreaHeight = mRemoteHostDesktop.getHeight() - safeInsets.top - safeInsets.bottom;
        DisplayMetrics metrics = getResources().getDisplayMetrics();
        safeAreaWidth = ChromotingUtil.pxToDp(metrics, safeAreaWidth);
        safeAreaHeight = ChromotingUtil.pxToDp(metrics, safeAreaHeight);
        mClient.sendClientResolution(safeAreaWidth, safeAreaHeight, metrics.density);
    }

    /**
     * Called once when a keyboard key is pressed, then again when that same key is released.
     * This is not guaranteed to be notified of all soft keyboard events: certain keyboards
     * might not call it at all, while others might skip it in certain situations (e.g. swipe
     * input).
     */
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            mClient.destroy();
            return super.dispatchKeyEvent(event);
        }

        return mInjector.sendKeyEvent(event);
    }
}
