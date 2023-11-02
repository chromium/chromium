// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_EVALUATE_D3D_H_
#define REMOTING_HOST_WIN_EVALUATE_D3D_H_

#include <string>
#include <vector>

namespace remoting {

// Evaluates the D3D capability of the system and sends the results to stdout.
// DO NOT call this method within the host process. Only call in an isolated
// child process. I.e. from EvaluateCapabilityLocally().
int EvaluateD3D();

// Evaluates the D3D capability of the system in a separate process. Returns
// true if the process succeeded. The capabilities will be stored in |result|.
// Note, this is not a cheap call, it uses EvaluateCapability() internally to
// spawn a new process, which may take a noticeable amount of time.
bool GetD3DCapabilities(std::vector<std::string>* result);

// Used to ensure that pulling in the DirectX dependencies and creating a D3D
// Device does not result in a crash or system instability.
// Note: This is an expensive call as it creates a new process and blocks on it.
bool IsD3DAvailable();

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_EVALUATE_D3D_H_
