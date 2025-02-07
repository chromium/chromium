/*
 * Copyright 2024 The Android Open Source Project
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
package androidx.pdf.viewer.loader;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.net.Uri;
import android.os.IBinder;
import androidx.annotation.RestrictTo;
import androidx.pdf.models.PdfDocumentRemote;
import androidx.pdf.service.PdfDocumentService;
import androidx.pdf.util.Preconditions;
import org.jspecify.annotations.NonNull;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;
/**
 * Handles the connection to the Pdf service:
 * <ul>
 * <li>Handles binding and the lifecycle of the connection,
 * <li>Manages the {@link PdfDocumentRemote} stub object.
 * </ul>
 */
@RestrictTo(RestrictTo.Scope.LIBRARY)
public class PdfConnection implements ServiceConnection {
    private static final String TAG = PdfConnection.class.getSimpleName();
    private static final int MAX_CONNECT_RETRIES = 3;
    private final Context mContext;
    private final Lock mLock = new ReentrantLock();
    private final Condition mIsBound = mLock.newCondition();
    private PdfDocumentRemote mPdfRemote;
    private int mNumCrashes = 0;
    private boolean mHasSuccessfullyConnectedEver = false;
    private boolean mIsLoaded = false;
    private String mCurrentTask = null;
    private boolean mConnected = false;
    private Runnable mOnConnect;
    private Runnable mOnConnectFailure;
    PdfConnection(Context ctx) {
        this.mContext = ctx;
    }
    /** Sets a {@link Runnable} to be run as soon as the service is (re-)connected. */
    public void setOnConnectInitializer(@NonNull Runnable onConnect) {
        this.mOnConnect = onConnect;
    }
    /** Sets a {@link Runnable} to be run if the service never successfully connects. */
    public void setConnectionFailureHandler(@NonNull Runnable onConnectFailure) {
        this.mOnConnectFailure = onConnectFailure;
    }
    /** Checks if Connection to PdfDocumentService is established */
    public boolean isConnected() {
        return mConnected;
    }
    /**
     * Returns a {@link PdfDocumentRemote} if the service is bound. It could be still initializing
     * (see {@link #setDocumentLoaded}).
     */
    public @NonNull PdfDocumentRemote getPdfDocument(@NonNull String forTask) {
        Preconditions.checkState(mCurrentTask == null, "already locked: " + mCurrentTask);
        mCurrentTask = forTask;
        return mPdfRemote;
    }
    /**
     * Releases the Pdf Remote.
     */
    public void releasePdfDocument() {
        mCurrentTask = null;
    }
    /** Returns whether {@link #getPdfDocument} is ready to accept tasks. */
    protected boolean isLoaded() {
        return mPdfRemote != null && mIsLoaded;
    }
    /**
     * This records that the document is loaded:
     * <ul>
     * <li>it is now ready to process tasks (until a disconnection notice happens),
     * <li>since we were able to load this document once, we should be able to load it again if
     * there is a problem and not run in a perpetual crash-restart loop.
     * </ul>
     */
    public void setDocumentLoaded() {
        if (mPdfRemote != null) {
            mHasSuccessfullyConnectedEver = true;
            mIsLoaded = true;
        }
    }
    @Override
    public void onServiceConnected(ComponentName name, IBinder service) {
        mConnected = true;
        mIsLoaded = false;
        mLock.lock();
        try {
            mPdfRemote = PdfDocumentRemote.Stub.asInterface(service);
            mIsBound.signal();
        } finally {
            mLock.unlock();
        }
        if (mOnConnect != null) {
            mOnConnect.run();
        }
    }
    @Override
    public void onServiceDisconnected(ComponentName name) {
        mIsLoaded = false;
        if (mCurrentTask != null) {
            // A task was in progress, we want to report the crash and restart the service.
            mNumCrashes++;
            TaskDenyList.maybeDenyListTask(mCurrentTask);
            // We have never connected to this document, and we have crashed repeatedly.
            if (!mHasSuccessfullyConnectedEver && mNumCrashes >= MAX_CONNECT_RETRIES) {
                disconnect();
                if (mOnConnectFailure != null) {
                    mOnConnectFailure.run();
                }
            }
        } else {
            // No task was in progress, probably just system cleaning up idle resources.
            disconnect();
        }
        // if disconnect() was not called, the system will try to restart the service, and when
        // it does,
        // onServiceConnected will be called again.
        mLock.lock();
        try {
            mPdfRemote = null;
        } finally {
            mLock.unlock();
        }
    }
    void connect(Uri uri) {
        if (mConnected) {
            return;
        }
        Intent intent = new Intent(mContext, PdfDocumentService.class);
        // Data is only required here to make sure we start a new service per document.
        intent.setData(uri);
        mContext.bindService(intent, this, Context.BIND_AUTO_CREATE);
    }
    void disconnect() {
        mLock.lock();
        try {
            if (mConnected) {
                mContext.unbindService(this);
                mConnected = false;
            }
        } finally {
            mLock.unlock();
        }
    }
}
