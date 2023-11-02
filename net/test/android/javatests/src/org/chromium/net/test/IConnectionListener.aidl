// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

interface IConnectionListener {
    /**
     * When a new connection has been accepted by the server.
     *
     * @param socketId Unique as long as the socket is alive.
     */
    oneway void acceptedSocket(long socketId);

    /**
     * When data has been read on a socket.
     *
     * @param socketId Unique as long as the socket is alive.
     */
    oneway void readFromSocket(long socketId);
}
