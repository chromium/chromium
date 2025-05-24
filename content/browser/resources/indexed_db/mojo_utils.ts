// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import type {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import type {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import type {Origin} from 'chrome://resources/mojo/url/mojom/origin.mojom-webui.js';

import type {SchemefulSite} from './schemeful_site.mojom-webui.js';

// Methods to convert mojo values to strings or to objects with readable
// toString values. Accessible to jstemplate html code.
export function time(mojoTime: Time): Date {
  // The JS Date() is based off of the number of milliseconds since
  // the UNIX epoch (1970-01-01 00::00:00 UTC), while |internalValue|
  // of the base::Time (represented in mojom.Time) represents the
  // number of microseconds since the Windows FILETIME epoch
  // (1601-01-01 00:00:00 UTC). This computes the final JS time by
  // computing the epoch delta and the conversion from microseconds to
  // milliseconds.
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  // |epochDeltaInMs| equals to
  // base::Time::kTimeTToMicrosecondsOffset.
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const timeInMs = Number(mojoTime.internalValue) / 1000;

  return new Date(timeInMs - epochDeltaInMs);
}

// Joins a list of Mojom strings to a comma separated JS string.
export function scope(mojoScope: String16[]): string {
  return `[${mojoScope.map(s => mojoString16ToString(s)).join(', ')}]`;
}

export function origin(mojoOrigin: Origin): string {
  const {scheme, host, port} = mojoOrigin;
  const portSuf = (port === 0 ? '' : `:${port}`);
  return `${scheme}://${host}${portSuf}`;
}

export function schemefulSite(mojoSite: SchemefulSite): string {
  return origin(mojoSite.siteAsOrigin);
}
