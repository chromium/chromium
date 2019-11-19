// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.jni;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chromoting.HostInfo;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Date;

/** Helper for fetching and modifying the host list. */
@JNINamespace("remoting")
public class DirectoryService {
    /** All callbacks can receive an error. */
    public interface CallbackBase { void onError(@DirectoryServiceRequestError int error); }

    /** Callback for receiving the host list */
    public interface HostListCallback extends CallbackBase {
        void onHostListReceived(HostInfo[] response);
    }

    /** Callback for receiving the delete result */
    public interface DeleteCallback extends CallbackBase { void onHostDeleted(); }

    public DirectoryService() {
        mNativeJniDirectoryService = DirectoryServiceJni.get().init();
    }

    /**
     * Causes the host list to be fetched on a background thread. This should be called on the
     * main thread, and callbacks will also be invoked on the main thread. On success,
     * callback.onHostListReceived() will be called, otherwise callback.onError() will be called
     * with an error-code describing the failure.
     */
    public void retrieveHostList(final HostListCallback callback) {
        DirectoryServiceJni.get().retrieveHostList(mNativeJniDirectoryService, callback);
    }

    /**
     * Deletes a host on the background thread. On success, callback.onHostUpdated() will be called,
     * otherwise callback.onError() will be called with an error-code describing the failure.
     */
    public void deleteHost(final String hostId, final DeleteCallback callback) {
        DirectoryServiceJni.get().deleteHost(mNativeJniDirectoryService, hostId, callback);
    }

    @Override
    public void finalize() {
        DirectoryServiceJni.get().destroy(mNativeJniDirectoryService);
    }

    private final long mNativeJniDirectoryService;

    @NativeMethods
    interface Natives {
        long init();
        void retrieveHostList(long nativeJniDirectoryService, HostListCallback callback);
        void deleteHost(long nativeJniDirectoryService, String hostId, DeleteCallback callback);
        void destroy(long nativeJniDirectoryService);
    }

    /**
     * Called by native code when the host list has been successfully retrieved.
     * @param callback The Java callback originally provided to retrieveHostList.
     * @param response The serialized response.
     */
    @CalledByNative
    static void onHostListRetrieved(final HostListCallback callback, final byte[] response) {
        remoting.apis.v1.GetHostListResponse hostListResponse;
        try {
            hostListResponse = remoting.apis.v1.GetHostListResponse.parseFrom(response);
        } catch (InvalidProtocolBufferException e) {
            callback.onError(DirectoryServiceRequestError.UNEXPECTED_RESPONSE);
            return;
        }

        final int count = hostListResponse.getHostsCount();

        final HostInfo[] hosts = new HostInfo[count];

        for (int i = 0; i < count; ++i) {
            remoting.apis.v1.HostInfo hostProto = hostListResponse.getHosts(i);
            HostInfo host = new HostInfo(hostProto.getHostName(), hostProto.getHostId(),
                    hostProto.getJabberId(), hostProto.getFtlId(), hostProto.getPublicKey(),
                    new ArrayList<String>(hostProto.getTokenUrlPatternList()),
                    hostProto.getStatus() == remoting.apis.v1.HostInfo.Status.ONLINE,
                    hostProto.getHostOfflineReason(), new Date(hostProto.getLastSeenTime()),
                    hostProto.getHostVersion(), hostProto.getHostOsName(),
                    hostProto.getHostOsVersion());
            hosts[i] = host;
        }

        Arrays.sort(hosts, new Comparator<HostInfo>() {
            @Override
            public int compare(HostInfo o1, HostInfo o2) {
                // Sort online hosts first.
                int result = -Boolean.compare(o1.isOnline, o2.isOnline);
                if (result == 0) {
                    result = String.CASE_INSENSITIVE_ORDER.compare(o1.name, o2.name);
                }
                return result;
            }
        });

        callback.onHostListReceived(hosts);
    }

    /**
     * Called by native code when the delete request was successfully completed.
     * @param callback The Java callback originally provided to deleteHost.
     */
    @CalledByNative
    static void onHostDeleted(final DeleteCallback callback) {
        callback.onHostDeleted();
    }

    /**
     * Called by native code when an error occurs while performing the request.
     * @param callback The Java callback originally provided to deleteHost.
     * @param error The type of error that occurred.
     */
    @CalledByNative
    static void onError(final CallbackBase callback, @DirectoryServiceRequestError int error) {
        callback.onError(error);
    }
}
