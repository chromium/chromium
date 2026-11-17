// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GDM_MANAGED_HOST_TYPE_H_
#define REMOTING_HOST_LINUX_GDM_MANAGED_HOST_TYPE_H_

namespace remoting {

class HostType;

// Returns the GDM-managed host type.
const HostType& GetGdmManagedHostType();

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GDM_MANAGED_HOST_TYPE_H_
