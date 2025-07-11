// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_MODEL_AIM_AVAILABILITY_H_
#define IOS_CHROME_BROWSER_AIM_MODEL_AIM_AVAILABILITY_H_

class PrefService;
class TemplateURLService;

/// Whether AIM is available.
bool IsAIMAvailable(const PrefService* prefs,
                    const TemplateURLService* template_url_service);

#endif  // IOS_CHROME_BROWSER_AIM_MODEL_AIM_AVAILABILITY_H_
