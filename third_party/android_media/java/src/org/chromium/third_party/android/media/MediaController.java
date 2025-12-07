/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.chromium.third_party.android.media;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.support.v4.media.session.PlaybackStateCompat;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.SeekBar;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Formatter;
import java.util.Locale;

/**
 * Helper for implementing media controls in an application.
 * We use this custom version instead of {@link android.widget.MediaController} so that we can
 * customize the look as we want. This file is taken directly from the public Android sample for
 * supportv4, with tiny bug fixes.
 */
@NullMarked
public class MediaController extends FrameLayout {
    /**
     * The interface that allows media controller to actually control media and provides some
     * essential metadata for the UI like the current position, duration, etc.
     */
    public interface Delegate {
        /**
         * Called when the user wants to resume or start.
         */
        void play();

        /**
         * Called when the user wants to pause.
         */
        void pause();

        /**
         * Called when the user wants to seek.
         */
        void seekTo(long pos);

        /**
         * @return the current media duration, in milliseconds.
         */
        long getDuration();

        /**
         * @return the current playback position, in milliseconds.
         */
        long getPosition();

        /**
         * @return if the media is currently playing.
         */
        boolean isPlaying();

        /**
         * @return a combination of {@link PlaybackStateCompat} flags defining what UI elements will
         *         be available to the user.
         */
        long getActionFlags();
    }

    private @Nullable Delegate mDelegate;
    private final Context mContext;
    private @Nullable ViewGroup mProgressGroup;
    private @Nullable SeekBar mProgressBar;
    private @Nullable TextView mEndTime;
    private @Nullable TextView mCurrentTime;
    private boolean mDragging;
    private final boolean mUseFastForward;
    private boolean mListenersSet;
    private boolean mShowNext;
    private boolean mShowPrev;
    private View.@Nullable OnClickListener mNextListener;
    private View.@Nullable OnClickListener mPrevListener;

    private @Nullable StringBuilder mFormatBuilder;

    private @Nullable Formatter mFormatter;

    private @Nullable ImageButton mPauseButton;
    private @Nullable ImageButton mFfwdButton;
    private @Nullable ImageButton mRewButton;
    private @Nullable ImageButton mNextButton;
    private @Nullable ImageButton mPrevButton;

    public MediaController(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mUseFastForward = true;
        LayoutInflater inflate =
                (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        inflate.inflate(R.layout.media_controller, this, true);
        initControllerView();
    }

    public MediaController(Context context, boolean useFastForward) {
        super(context);
        mContext = context;
        mUseFastForward = useFastForward;
    }

    public MediaController(Context context) {
        this(context, true);
    }

    public void setDelegate(Delegate delegate) {
        mDelegate = delegate;
        updatePausePlay();
    }

    private void initControllerView() {
        mPauseButton = (ImageButton) findViewById(R.id.pause);
        if (mPauseButton != null) {
            mPauseButton.requestFocus();
            mPauseButton.setOnClickListener(mPauseListener);
        }

        mFfwdButton = (ImageButton) findViewById(R.id.ffwd);
        if (mFfwdButton != null) {
            mFfwdButton.setOnClickListener(mFfwdListener);
            mFfwdButton.setVisibility(mUseFastForward ? View.VISIBLE : View.GONE);
        }

        mRewButton = (ImageButton) findViewById(R.id.rew);
        if (mRewButton != null) {
            mRewButton.setOnClickListener(mRewListener);
            mRewButton.setVisibility(mUseFastForward ? View.VISIBLE : View.GONE);
        }

        // By default these are hidden. They will be enabled when setPrevNextListeners() is called
        mNextButton = (ImageButton) findViewById(R.id.next);
        if (mNextButton != null && !mListenersSet) {
            mNextButton.setVisibility(View.GONE);
        }
        mPrevButton = (ImageButton) findViewById(R.id.prev);
        if (mPrevButton != null && !mListenersSet) {
            mPrevButton.setVisibility(View.GONE);
        }

        mProgressGroup = (ViewGroup) findViewById(R.id.mediacontroller_progress_container);

        if (mProgressGroup != null) {
            mProgressBar = (SeekBar) mProgressGroup.findViewById(R.id.mediacontroller_progress_bar);
            if (mProgressBar != null) {
                mProgressBar.setOnSeekBarChangeListener(mSeekListener);
                mProgressBar.setMax(1000);
            }
        }

        mEndTime = (TextView) findViewById(R.id.time);
        mCurrentTime = (TextView) findViewById(R.id.time_current);
        mFormatBuilder = new StringBuilder();
        mFormatter = new Formatter(mFormatBuilder, Locale.getDefault());

        installPrevNextListeners();
    }

    /**
     * Disable pause or seek buttons if the stream cannot be paused or seeked.
     * This requires the control interface to be a MediaPlayerControlExt
     */
    void updateButtons() {
        if (mDelegate == null) return;

        long flags = mDelegate.getActionFlags();
        boolean enabled = isEnabled();
        if (mPauseButton != null) {
            boolean needPlayPauseButton = (flags & PlaybackStateCompat.ACTION_PLAY) != 0
                    || (flags & PlaybackStateCompat.ACTION_PAUSE) != 0;
            mPauseButton.setEnabled(enabled && needPlayPauseButton);
        }
        if (mRewButton != null) {
            mRewButton.setEnabled(enabled && (flags & PlaybackStateCompat.ACTION_REWIND) != 0);
        }
        if (mFfwdButton != null) {
            mFfwdButton.setEnabled(
                    enabled && (flags & PlaybackStateCompat.ACTION_FAST_FORWARD) != 0);
        }
        if (mPrevButton != null) {
            mShowPrev =
                    (flags & PlaybackStateCompat.ACTION_SKIP_TO_NEXT) != 0 || mPrevListener != null;
            mPrevButton.setEnabled(enabled && mShowPrev);
        }
        if (mNextButton != null) {
            mShowNext = (flags & PlaybackStateCompat.ACTION_SKIP_TO_PREVIOUS) != 0
                    || mNextListener != null;
            mNextButton.setEnabled(enabled && mShowNext);
        }
    }

    public void refresh() {
        updateProgress();
        updateButtons();
        updatePausePlay();
    }

    private String stringForTime(int timeMs) {
        int totalSeconds = timeMs / 1000;

        int seconds = totalSeconds % 60;
        int minutes = (totalSeconds / 60) % 60;
        int hours = totalSeconds / 3600;

        assumeNonNull(mFormatBuilder).setLength(0);
        if (hours > 0) {
            return assumeNonNull(mFormatter)
                    .format("%d:%02d:%02d", hours, minutes, seconds)
                    .toString();
        } else {
            return assumeNonNull(mFormatter).format("%02d:%02d", minutes, seconds).toString();
        }
    }

    public long updateProgress() {
        if (mDelegate == null || mDragging) return 0;

        long position = mDelegate.getPosition();
        long duration = mDelegate.getDuration();
        if (duration <= 0) {
            // If there is no valid duration, hide the progress bar and time indicators.
            if (mProgressGroup != null) mProgressGroup.setVisibility(View.INVISIBLE);
        } else if (mProgressBar != null) {
            if (mProgressGroup != null) mProgressGroup.setVisibility(View.VISIBLE);
            // use long to avoid overflow
            long pos = 1000L * position / duration;
            mProgressBar.setProgress((int) pos);
            mProgressBar.setSecondaryProgress((int) pos);
        }

        if (mEndTime != null) mEndTime.setText(stringForTime((int) duration));
        if (mCurrentTime != null) mCurrentTime.setText(stringForTime((int) position));

        return position;
    }

    private final View.OnClickListener mPauseListener =
            new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    doPauseResume();
                }
            };

    private void updatePausePlay() {
        if (mDelegate == null || mPauseButton == null) return;

        if (mDelegate.isPlaying()) {
            mPauseButton.setImageResource(android.R.drawable.ic_media_pause);
        } else {
            mPauseButton.setImageResource(android.R.drawable.ic_media_play);
        }
    }

    private void doPauseResume() {
        if (mDelegate == null) return;

        if (mDelegate.isPlaying()) {
            mDelegate.pause();
        } else {
            mDelegate.play();
        }
        updatePausePlay();
    }

    // There are two scenarios that can trigger the seekbar listener to trigger:
    //
    // The first is the user using the touchpad to adjust the posititon of the
    // seekbar's thumb. In this case onStartTrackingTouch is called followed by
    // a number of onProgressChanged notifications, concluded by onStopTrackingTouch.
    // We're setting the field "mDragging" to true for the duration of the dragging
    // session to avoid jumps in the position in case of ongoing playback.
    //
    // The second scenario involves the user operating the scroll ball, in this
    // case there WON'T BE onStartTrackingTouch/onStopTrackingTouch notifications,
    // we will simply apply the updated position without suspending regular updates.
    private final SeekBar.OnSeekBarChangeListener mSeekListener =
            new SeekBar.OnSeekBarChangeListener() {
                @Override
                public void onStartTrackingTouch(SeekBar bar) {
                    mDragging = true;
                }

                @Override
                public void onProgressChanged(SeekBar bar, int progress, boolean fromuser) {
                    if (mDelegate == null) return;

                    if (!fromuser) {
                        // We're not interested in programmatically generated changes to
                        // the progress bar's position.
                        return;
                    }

                    long duration = mDelegate.getDuration();
                    long newposition = (duration * progress) / 1000L;
                    mDelegate.seekTo(newposition);
                    if (mCurrentTime != null)
                        mCurrentTime.setText(stringForTime((int) newposition));
                }

                @Override
                public void onStopTrackingTouch(SeekBar bar) {
                    mDragging = false;
                    updateProgress();
                    updatePausePlay();
                }
            };

    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);
        updateButtons();
    }

    @Override
    public void onInitializeAccessibilityEvent(AccessibilityEvent event) {
        super.onInitializeAccessibilityEvent(event);
        event.setClassName(MediaController.class.getName());
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        super.onInitializeAccessibilityNodeInfo(info);
        info.setClassName(MediaController.class.getName());
    }

    private final View.OnClickListener mRewListener =
            new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    if (mDelegate == null) return;

                    long pos = mDelegate.getPosition();
                    pos -= 5000; // milliseconds
                    mDelegate.seekTo(pos);
                    updateProgress();
                }
            };

    private final View.OnClickListener mFfwdListener =
            new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    if (mDelegate == null) return;

                    long pos = mDelegate.getPosition();
                    pos += 15000; // milliseconds
                    mDelegate.seekTo(pos);
                    updateProgress();
                }
            };

    private void installPrevNextListeners() {
        if (mNextButton != null) {
            mNextButton.setOnClickListener(mNextListener);
            mNextButton.setEnabled(mShowNext);
        }

        if (mPrevButton != null) {
            mPrevButton.setOnClickListener(mPrevListener);
            mPrevButton.setEnabled(mShowPrev);
        }
    }

    public void setPrevNextListeners(View.OnClickListener next, View.OnClickListener prev) {
        mNextListener = next;
        mPrevListener = prev;
        mListenersSet = true;

        installPrevNextListeners();

        if (mNextButton != null) {
            mNextButton.setVisibility(View.VISIBLE);
            mShowNext = true;
        }
        if (mPrevButton != null) {
            mPrevButton.setVisibility(View.VISIBLE);
            mShowPrev = true;
        }
    }
}
