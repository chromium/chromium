// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_iconset.js';

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';

const div = document.createElement('div');
div.innerHTML = getTrustedHTML`
<cr-iconset name="flags" size="24">
  <svg>
    <defs>
      <g id="file-upload-old" viewBox="0 -960 960 960">
        <path d="M440-320v-326L336-542l-56-58 200-200 200 200-56 58-104-104v326h-80ZM240-160q-33 0-56.5-23.5T160-240v-120h80v120h480v-120h80v120q0 33-23.5 56.5T720-160H240Z"></path>
      </g>
      <g id="upload">
        <path d="M6.594 19.2c-.496 0-.918-.177-1.27-.528a1.749 1.749 0 0 1-.523-1.274V16.5c0-.254.086-.469.258-.64a.863.863 0 0 1 .636-.258c.254 0 .47.085.64.257a.864.864 0 0 1 .267.641v.898h10.796V16.5c0-.254.086-.469.258-.64a.87.87 0 0 1 .637-.258c.254 0 .469.085.645.257a.86.86 0 0 1 .261.641v.898c0 .497-.176.922-.527 1.274a1.744 1.744 0 0 1-1.274.527Zm4.508-12.15-2 2a.879.879 0 0 1-.641.274.915.915 0 0 1-.91-.91c0-.242.09-.457.273-.64l3.551-3.547a.885.885 0 0 1 .629-.25c.121 0 .234.02.34.062.105.04.199.102.281.188l3.55 3.546a.864.864 0 0 1 .012 1.25.92.92 0 0 1-.648.278.883.883 0 0 1-.64-.278l-2-1.972v7.648a.884.884 0 0 1-.895.903.866.866 0 0 1-.64-.262.86.86 0 0 1-.262-.64Zm0 0"></path>
      </g>
    </defs>
  </svg>
</cr-iconset>`;

const iconsets = div.querySelectorAll('cr-iconset');
for (const iconset of iconsets) {
  document.head.appendChild(iconset);
}
