// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import static org.chromium.components.webauthn.AndroidApiCallMetrics.ApiCallEvent.CANCELLED;
import static org.chromium.components.webauthn.AndroidApiCallMetrics.ApiCallEvent.FAILED;
import static org.chromium.components.webauthn.AndroidApiCallMetrics.ApiCallEvent.STARTED;
import static org.chromium.components.webauthn.AndroidApiCallMetrics.ApiCallEvent.SUCCEEDED;
import static org.chromium.components.webauthn.AndroidApiCallMetrics.ApiCallEvent.TIMED_OUT;
import static org.chromium.components.webauthn.Fido2ApiCall.METHOD_APP_ISUVPAA;
import static org.chromium.components.webauthn.Fido2ApiCall.METHOD_APP_REGISTER;
import static org.chromium.components.webauthn.Fido2ApiCall.METHOD_APP_SIGN;
import static org.chromium.components.webauthn.Fido2ApiCall.METHOD_BROWSER_GETCREDENTIALS;
import static org.chromium.components.webauthn.Fido2ApiCall.METHOD_BROWSER_HYBRID_SIGN;
import static org.chromium.components.webauthn.Fido2ApiCall.METHOD_BROWSER_ISUVPAA;
import static org.chromium.components.webauthn.Fido2ApiCall.METHOD_BROWSER_REGISTER;
import static org.chromium.components.webauthn.Fido2ApiCall.METHOD_BROWSER_SIGN;
import static org.chromium.components.webauthn.Fido2ApiCall.METHOD_GET_LINK_INFO;
import static org.chromium.components.webauthn.Fido2ApiCall.METHOD_LIST_PASSKEYS_FOR_RP;

import android.app.PendingIntent;
import android.os.Parcel;

import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.Status;
import com.google.android.gms.tasks.Task;
import com.google.android.gms.tasks.TaskCompletionSource;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

@RunWith(BaseRobolectricTestRunner.class)
public class Fido2ApiRobolectricTest {
    private static final byte[] sCredentialId = new byte[] {1, 2, 3, 4};
    private static final String HISTOGRAM_PREFIX = "WebAuthentication.Android.GmsCoreApiCall.";

    @Test
    public void testParseCredentialList_fromCacheIsDiscoverableTrue() {
        Parcel p = Parcel.obtain();
        p.writeInt(1); // One credential in list.
        writeParcelable(
                p, "com.google.android.gms.fido.fido2.api.common.DiscoverableCredentialInfo");
        writeCredential(p, sCredentialId, /* isDiscoverable= */ true);

        p.setDataPosition(0);
        List<WebauthnCredentialDetails> credentials =
                Fido2Api.parseCredentialList(p, /* fromCache= */ true);
        assertThat(credentials).hasSize(1);
        assertThat(credentials.get(0).mIsDiscoverable).isTrue();
        assertThat(credentials.get(0).mCredentialId).isEqualTo(sCredentialId);
    }

    @Test
    public void testParseCredentialList_notFromCacheIsDiscoverableTrue() {
        Parcel p = Parcel.obtain();
        p.writeInt(1); // One credential in list.
        writeParcelable(
                p, "com.google.android.gms.fido.fido2.api.common.DiscoverableCredentialInfo");
        writeCredential(p, sCredentialId, /* isDiscoverable= */ true);

        p.setDataPosition(0);
        List<WebauthnCredentialDetails> credentials =
                Fido2Api.parseCredentialList(p, /* fromCache= */ false);
        assertThat(credentials).hasSize(1);
        assertThat(credentials.get(0).mIsDiscoverable).isTrue();
        assertThat(credentials.get(0).mCredentialId).isEqualTo(sCredentialId);
    }

    @Test
    public void testParseCredentialList_notFromCacheIsDiscoverableFalse() {
        Parcel p = Parcel.obtain();
        p.writeInt(1); // One credential in list.
        writeParcelable(
                p, "com.google.android.gms.fido.fido2.api.common.DiscoverableCredentialInfo");
        writeCredential(p, sCredentialId, /* isDiscoverable= */ false);

        p.setDataPosition(0);
        List<WebauthnCredentialDetails> credentials =
                Fido2Api.parseCredentialList(p, /* fromCache= */ false);
        assertThat(credentials).hasSize(1);
        assertThat(credentials.get(0).mIsDiscoverable).isFalse();
        assertThat(credentials.get(0).mCredentialId).isEqualTo(sCredentialId);
    }

    @Test
    public void testBinderCallMetrics_succeeded() {
        PendingIntent intent = Mockito.mock(PendingIntent.class);
        assertTaskOutcome(METHOD_APP_ISUVPAA, "AppIsUvpaa", false, SUCCEEDED);
        assertTaskOutcome(METHOD_APP_REGISTER, "AppRegister", intent, SUCCEEDED);
        assertTaskOutcome(METHOD_APP_SIGN, "AppSign", intent, SUCCEEDED);
        assertTaskOutcome(
                METHOD_BROWSER_GETCREDENTIALS, "BrowserGetCredentials", List.of(), SUCCEEDED);
        assertTaskOutcome(METHOD_BROWSER_HYBRID_SIGN, "BrowserHybridSign", intent, SUCCEEDED);
        assertTaskOutcome(METHOD_BROWSER_ISUVPAA, "BrowserIsUvpaa", true, SUCCEEDED);
        assertTaskOutcome(METHOD_BROWSER_REGISTER, "BrowserRegister", intent, SUCCEEDED);
        assertTaskOutcome(METHOD_BROWSER_SIGN, "BrowserSign", intent, SUCCEEDED);
        // A null result is only a failure for methods that return a PendingIntent.
        assertTaskOutcome(METHOD_GET_LINK_INFO, "FirstPartyGetLinkInfo", null, SUCCEEDED);
        assertTaskOutcome(
                METHOD_LIST_PASSKEYS_FOR_RP, "PersistentListPasskeysForRp", List.of(), SUCCEEDED);
    }

    @Test
    public void testBinderCallMetrics_nullPendingIntent_failed() {
        assertTaskOutcome(METHOD_APP_REGISTER, "AppRegister", null, FAILED);
        assertTaskOutcome(METHOD_APP_SIGN, "AppSign", null, FAILED);
        assertTaskOutcome(METHOD_BROWSER_HYBRID_SIGN, "BrowserHybridSign", null, FAILED);
        assertTaskOutcome(METHOD_BROWSER_REGISTER, "BrowserRegister", null, FAILED);
        assertTaskOutcome(METHOD_BROWSER_SIGN, "BrowserSign", null, FAILED);
    }

    @Test
    public void testBinderCallMetrics_taskExceptions() {
        assertTaskOutcome(
                METHOD_BROWSER_SIGN,
                "BrowserSign",
                new ApiException(Status.RESULT_TIMEOUT),
                TIMED_OUT);
        assertTaskOutcome(
                METHOD_BROWSER_SIGN,
                "BrowserSign",
                new ApiException(Status.RESULT_CANCELED),
                CANCELLED);
        assertTaskOutcome(
                METHOD_BROWSER_SIGN,
                "BrowserSign",
                new ApiException(Status.RESULT_INTERNAL_ERROR),
                FAILED);
        assertTaskOutcome(
                METHOD_LIST_PASSKEYS_FOR_RP,
                "PersistentListPasskeysForRp",
                new IllegalArgumentException("corrupt parcel"),
                FAILED);
    }

    @Test
    public void testBinderCallMetrics_unresolvedTask_recordsOnlyStarted() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(HISTOGRAM_PREFIX + "BrowserSign", STARTED);
        new TestFido2ApiCall(new TaskCompletionSource<PendingIntent>().getTask())
                .runMethod(METHOD_BROWSER_SIGN);
        watcher.assertExpected();
    }

    @Test
    public void testBinderCallMetrics_synchronousThrow_recordsFailedAndRethrows() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(HISTOGRAM_PREFIX + "BrowserSign", STARTED, FAILED)
                        .build();
        TestFido2ApiCall call = new TestFido2ApiCall(new IllegalStateException());
        assertThrows(IllegalStateException.class, () -> call.runMethod(METHOD_BROWSER_SIGN));
        watcher.assertExpected();
    }

    @Test
    public void testBinderCallMetrics_synchronousThrowOffUiThread_recordsFailedOnUiThread()
            throws InterruptedException {
        // CableAuthenticatorModuleProvider calls run(METHOD_GET_LINK_INFO) on a background thread.
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                HISTOGRAM_PREFIX + "FirstPartyGetLinkInfo", STARTED, FAILED)
                        .build();
        TestFido2ApiCall call = new TestFido2ApiCall(new IllegalStateException());
        AtomicReference<Throwable> thrown = new AtomicReference<>();
        Thread thread =
                new Thread(
                        () -> {
                            try {
                                call.runMethod(METHOD_GET_LINK_INFO);
                            } catch (IllegalStateException e) {
                                thrown.set(e);
                            }
                        });
        thread.start();
        thread.join();
        assertThat(thrown.get()).isInstanceOf(IllegalStateException.class);
        // FAILED is posted to the UI thread.
        ShadowLooper.idleMainLooper();
        watcher.assertExpected();
    }

    @Test
    public void testDeferredRecorder_finishBeforeMarkStarted_recordsOneLifecycle() {
        // A PendingIntent result can be delivered synchronously from within showIntent, before the
        // caller marks the session as started.
        String histogram = HISTOGRAM_PREFIX + "SignPendingIntent";
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(histogram, STARTED, CANCELLED)
                        .build();
        AndroidApiCallMetrics.DeferredOneShotRecorder recorder =
                AndroidApiCallMetrics.deferredCall(histogram);
        recorder.finish(CANCELLED);
        recorder.markStarted();
        recorder.finish(SUCCEEDED);
        watcher.assertExpected();
    }

    private static void assertTaskOutcome(
            int methodId, String operation, Object resultOrException, int expectedOutcome) {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(HISTOGRAM_PREFIX + operation, STARTED, expectedOutcome)
                        .build();
        TaskCompletionSource<Object> tcs = new TaskCompletionSource<>();
        new TestFido2ApiCall(tcs.getTask()).runMethod(methodId);
        if (resultOrException instanceof Exception exception) {
            tcs.setException(exception);
        } else {
            tcs.setResult(resultOrException);
        }
        watcher.assertExpected();
    }

    /** A Fido2ApiCall whose Binder call returns a fixed Task or throws synchronously. */
    private static final class TestFido2ApiCall extends Fido2ApiCall {
        private final Task<?> mTask;
        private final RuntimeException mException;

        TestFido2ApiCall(Task<?> task) {
            this(task, null);
        }

        TestFido2ApiCall(RuntimeException exception) {
            this(null, exception);
        }

        private TestFido2ApiCall(Task<?> task, RuntimeException exception) {
            super(ContextUtils.getApplicationContext(), Fido2ApiCall.BROWSER_API);
            mTask = task;
            mException = exception;
        }

        @Override
        @SuppressWarnings("unchecked")
        <Result> Task<Result> runBinderCall(
                int methodId, int transactionId, Parcel args, Callback<Result> callback) {
            if (mException != null) {
                throw mException;
            }
            return (Task<Result>) mTask;
        }

        void runMethod(int methodId) {
            run(methodId, /* transactionId= */ 0, Parcel.obtain(), (cs) -> {});
        }
    }

    private void writeCredential(Parcel p, byte[] credentialId, boolean isDiscoverable) {
        final int objectStart = writeHeader(20293, p); // OBJECT_MAGIC
        final int credIdStart = writeHeader(4, p);
        p.writeByteArray(credentialId);
        writeLength(credIdStart, p);

        final int userNameStart = writeHeader(1, p);
        p.writeString("name");
        writeLength(userNameStart, p);
        final int userDisplayNameStart = writeHeader(2, p);
        p.writeString("displayName");
        writeLength(userDisplayNameStart, p);
        final int userIdStart = writeHeader(3, p);
        p.writeByteArray(new byte[] {1});
        writeLength(userIdStart, p);

        final int isDiscoverableStart = writeHeader(5, p);
        p.writeInt(isDiscoverable ? 1 : 0);
        writeLength(isDiscoverableStart, p);

        writeLength(objectStart, p);
    }

    private static int writeHeader(int tag, Parcel parcel) {
        parcel.writeInt(0xffff0000 | tag);
        return startLength(parcel);
    }

    private static int startLength(Parcel parcel) {
        int pos = parcel.dataPosition();
        parcel.writeInt(0xdddddddd);
        return pos;
    }

    private static void writeLength(int pos, Parcel parcel) {
        int totalLength = parcel.dataPosition();
        parcel.setDataPosition(pos);
        parcel.writeInt(totalLength - pos - 4);
        parcel.setDataPosition(totalLength);
    }

    private static boolean doesParcelUseLengthPrefix() {
        Parcel parcel = Parcel.obtain();
        parcel.writeValue(new ArrayList<>());
        boolean ret = parcel.dataPosition() == 12;
        parcel.recycle();
        return ret;
    }

    private void writeParcelable(Parcel p, String className) {
        p.writeInt(4); // VAL_PARCELABLE
        if (doesParcelUseLengthPrefix()) {
            p.writeInt(0); // length prefix; ignored by Fido2Api.parseCredentialList
        }
        p.writeString(className);
    }
}
