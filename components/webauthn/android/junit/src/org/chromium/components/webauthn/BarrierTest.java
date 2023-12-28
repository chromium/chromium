// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRule;

import java.util.Arrays;
import java.util.Collection;

@RunWith(ParameterizedRobolectricTestRunner.class)
public class BarrierTest {
    public enum ApiCallType {
        NONE,
        CRED_MAN,
        FIDO_2_API,
    }

    public enum ApiCallStatus {
        NONE,
        SUCCESS,
        FAILURE,
    }

    public enum Expectation {
        NONE,
        CRED_MAN_RAN,
        FIDO_2_API_RAN,
        BOTH_RAN,
        ERROR_RAN,
    }

    @Parameters
    public static Collection<Object[]> data() {
        return Arrays.asList(
                new Object[][] {
                    {
                        /* mode= */ Barrier.Mode.ONLY_CRED_MAN,
                        /* firstCompletedApi= */ ApiCallType.CRED_MAN,
                        /* firstCompletedStatus= */ ApiCallStatus.SUCCESS,
                        /* secondCompletion= */ ApiCallType.NONE,
                        /* secondCompletionType= */ ApiCallStatus.NONE,
                        /* expectation= */ Expectation.CRED_MAN_RAN
                    },
                    {
                        /* mode= */ Barrier.Mode.ONLY_CRED_MAN,
                        /* firstCompletedApi= */ ApiCallType.CRED_MAN,
                        /* firstCompletedStatus= */ ApiCallStatus.FAILURE,
                        /* secondCompletion= */ ApiCallType.NONE,
                        /* secondCompletionType= */ ApiCallStatus.NONE,
                        /* expectation= */ Expectation.ERROR_RAN
                    },
                    {
                        /* mode= */ Barrier.Mode.ONLY_FIDO_2_API,
                        /* firstCompletion= */ ApiCallType.FIDO_2_API,
                        /* firstCompletionType= */ ApiCallStatus.SUCCESS,
                        /* secondCompletion= */ ApiCallType.NONE,
                        /* secondCompletionType= */ ApiCallStatus.NONE,
                        /* expectation= */ Expectation.FIDO_2_API_RAN
                    },
                    {
                        /* mode= */ Barrier.Mode.ONLY_FIDO_2_API,
                        /* firstCompletion= */ ApiCallType.FIDO_2_API,
                        /* firstCompletionType= */ ApiCallStatus.FAILURE,
                        /* secondCompletion= */ ApiCallType.NONE,
                        /* secondCompletionType= */ ApiCallStatus.NONE,
                        /* expectation= */ Expectation.ERROR_RAN
                    },
                    {
                        /* mode= */ Barrier.Mode.BOTH,
                        /* firstCompletion= */ ApiCallType.FIDO_2_API,
                        /* firstCompletionType= */ ApiCallStatus.SUCCESS,
                        /* secondCompletion= */ ApiCallType.CRED_MAN,
                        /* secondCompletionType= */ ApiCallStatus.SUCCESS,
                        /* expectation= */ Expectation.BOTH_RAN
                    },
                    {
                        /* mode= */ Barrier.Mode.BOTH,
                        /* firstCompletion= */ ApiCallType.FIDO_2_API,
                        /* firstCompletionType= */ ApiCallStatus.SUCCESS,
                        /* secondCompletion= */ ApiCallType.CRED_MAN,
                        /* secondCompletionType= */ ApiCallStatus.FAILURE,
                        /* expectation= */ Expectation.FIDO_2_API_RAN
                    },
                    {
                        /* mode= */ Barrier.Mode.BOTH,
                        /* firstCompletion= */ ApiCallType.FIDO_2_API,
                        /* firstCompletionType= */ ApiCallStatus.SUCCESS,
                        /* secondCompletion= */ ApiCallType.NONE,
                        /* secondCompletionType= */ ApiCallStatus.NONE,
                        /* expectation= */ Expectation.NONE
                    },
                    {
                        /* mode= */ Barrier.Mode.BOTH,
                        /* firstCompletion= */ ApiCallType.FIDO_2_API,
                        /* firstCompletionType= */ ApiCallStatus.FAILURE,
                        /* secondCompletion= */ ApiCallType.CRED_MAN,
                        /* secondCompletionType= */ ApiCallStatus.SUCCESS,
                        /* expectation= */ Expectation.CRED_MAN_RAN
                    },
                    {
                        /* mode= */ Barrier.Mode.BOTH,
                        /* firstCompletion= */ ApiCallType.FIDO_2_API,
                        /* firstCompletionType= */ ApiCallStatus.FAILURE,
                        /* secondCompletion= */ ApiCallType.CRED_MAN,
                        /* secondCompletionType= */ ApiCallStatus.FAILURE,
                        /* expectation= */ Expectation.ERROR_RAN
                    },
                    {
                        /* mode= */ Barrier.Mode.BOTH,
                        /* firstCompletion= */ ApiCallType.CRED_MAN,
                        /* firstCompletionType= */ ApiCallStatus.SUCCESS,
                        /* secondCompletion= */ ApiCallType.FIDO_2_API,
                        /* secondCompletionType= */ ApiCallStatus.SUCCESS,
                        /* expectation= */ Expectation.BOTH_RAN
                    },
                    {
                        /* mode= */ Barrier.Mode.BOTH,
                        /* firstCompletion= */ ApiCallType.CRED_MAN,
                        /* firstCompletionType= */ ApiCallStatus.SUCCESS,
                        /* secondCompletion= */ ApiCallType.FIDO_2_API,
                        /* secondCompletionType= */ ApiCallStatus.FAILURE,
                        /* expectation= */ Expectation.CRED_MAN_RAN
                    },
                    {
                        /* mode= */ Barrier.Mode.BOTH,
                        /* firstCompletion= */ ApiCallType.CRED_MAN,
                        /* firstCompletionType= */ ApiCallStatus.SUCCESS,
                        /* secondCompletion= */ ApiCallType.NONE,
                        /* secondCompletionType= */ ApiCallStatus.NONE,
                        /* expectation= */ Expectation.NONE
                    },
                    {
                        /* mode= */ Barrier.Mode.BOTH,
                        /* firstCompletion= */ ApiCallType.CRED_MAN,
                        /* firstCompletionType= */ ApiCallStatus.FAILURE,
                        /* secondCompletion= */ ApiCallType.FIDO_2_API,
                        /* secondCompletionType= */ ApiCallStatus.SUCCESS,
                        /* expectation= */ Expectation.FIDO_2_API_RAN
                    },
                    {
                        /* mode= */ Barrier.Mode.BOTH,
                        /* firstCompletion= */ ApiCallType.CRED_MAN,
                        /* firstCompletionType= */ ApiCallStatus.FAILURE,
                        /* secondCompletion= */ ApiCallType.FIDO_2_API,
                        /* secondCompletionType= */ ApiCallStatus.FAILURE,
                        /* expectation= */ Expectation.ERROR_RAN
                    }
                });
    }

    @Parameter(0)
    public Barrier.Mode mMode;

    @Parameter(1)
    public ApiCallType mFirstCompletedApi;

    @Parameter(2)
    public ApiCallStatus mFirstCompletedStatus;

    @Parameter(3)
    public ApiCallType mSecondCompletedApi;

    @Parameter(4)
    public ApiCallStatus mSecondCompletedStatus;

    @Parameter(5)
    public Expectation mExpectation;

    @Rule(order = -2)
    public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Mock Runnable mCredManSuccesfulRunnable;
    @Mock Runnable mFido2ApiSuccessfulRunnable;
    @Mock Callback<Integer> mErrorCallback;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.openMocks(this);
    }

    @Test
    public void testScenarios() {
        Barrier barrier = new Barrier(mErrorCallback);
        barrier.resetAndSetWaitStatus(mMode);

        if (mFirstCompletedApi == ApiCallType.CRED_MAN
                && mFirstCompletedStatus == ApiCallStatus.SUCCESS) {
            barrier.onCredManSuccessful(mCredManSuccesfulRunnable);
        } else if (mFirstCompletedApi == ApiCallType.CRED_MAN
                && mFirstCompletedStatus == ApiCallStatus.FAILURE) {
            barrier.onCredManFailed(0);
        } else if (mFirstCompletedApi == ApiCallType.FIDO_2_API
                && mFirstCompletedStatus == ApiCallStatus.SUCCESS) {
            barrier.onFido2ApiSuccessful(mFido2ApiSuccessfulRunnable);
        } else {
            barrier.onFido2ApiFailed(0);
        }

        if (mSecondCompletedApi == ApiCallType.CRED_MAN
                && mSecondCompletedStatus == ApiCallStatus.SUCCESS) {
            barrier.onCredManSuccessful(mCredManSuccesfulRunnable);
        } else if (mSecondCompletedApi == ApiCallType.CRED_MAN
                && mSecondCompletedStatus == ApiCallStatus.FAILURE) {
            barrier.onCredManFailed(0);
        } else if (mSecondCompletedApi == ApiCallType.FIDO_2_API
                && mSecondCompletedStatus == ApiCallStatus.SUCCESS) {
            barrier.onFido2ApiSuccessful(mFido2ApiSuccessfulRunnable);
        } else if (mSecondCompletedApi == ApiCallType.FIDO_2_API
                && mSecondCompletedStatus == ApiCallStatus.FAILURE) {
            barrier.onFido2ApiFailed(0);
        }

        switch (mExpectation) {
            case BOTH_RAN:
                verify(mFido2ApiSuccessfulRunnable, times(1)).run();
                verify(mCredManSuccesfulRunnable, times(1)).run();
                verify(mErrorCallback, times(0)).onResult(anyInt());
                break;
            case ERROR_RAN:
                verify(mFido2ApiSuccessfulRunnable, times(0)).run();
                verify(mCredManSuccesfulRunnable, times(0)).run();
                verify(mErrorCallback, times(1)).onResult(anyInt());
                break;
            case FIDO_2_API_RAN:
                verify(mFido2ApiSuccessfulRunnable, times(1)).run();
                verify(mCredManSuccesfulRunnable, times(0)).run();
                verify(mErrorCallback, times(0)).onResult(anyInt());
                break;
            case CRED_MAN_RAN:
                verify(mFido2ApiSuccessfulRunnable, times(0)).run();
                verify(mCredManSuccesfulRunnable, times(1)).run();
                verify(mErrorCallback, times(0)).onResult(anyInt());
                break;
            case NONE:
                verify(mFido2ApiSuccessfulRunnable, times(0)).run();
                verify(mCredManSuccesfulRunnable, times(0)).run();
                verify(mErrorCallback, times(0)).onResult(anyInt());
                break;
            default:
                assert false : "Unhandled expectation " + mExpectation;
        }
    }
}
