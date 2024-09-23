// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_CONTEXT_H_

#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"

namespace content {

// ResourceContext contains the relevant context information required for
// resource loading. It lives on the IO thread, although it is constructed on
// the UI thread. It must be destructed on the IO thread.
// TODO(crbug.com/40604019): Get rid of this class.
class CONTENT_EXPORT ResourceContext : public base::SupportsUserData {
 public:
  ResourceContext();
  ~ResourceContext() override;
  base::WeakPtr<ResourceContext> GetWeakPtr();

 private:
  base::WeakPtrFactory<ResourceContext> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_CONTEXT_H_
