# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkbuild.name_style_converter import NameStyleConverter


def mediaFeatureSymbol(entry, suffix):
    name = entry['name'].original
    if name.startswith('-webkit-'):
        name = name[8:]
    return 'k' + NameStyleConverter(name).to_upper_camel_case() + suffix


def getMediaFeatureSymbolWithSuffix(suffix):
    def returnedFunction(entry):
        return mediaFeatureSymbol(entry, suffix)

    return returnedFunction
