// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function result(message) {
    document.title = message;
}

function unexpectedErrorCallback()
{
  result('fail - unexpected error callback');
}

function unexpectedAbortCallback()
{
  result('fail - unexpected abort callback');
}

function unexpectedCompleteCallback()
{
  result('fail - unexpected complete callback');
}

function unexpectedSuccessCallback()
{
  result('fail - unexpected success callback');
}

function unexpectedUpgradeNeededCallback()
{
  result('fail - unexpected upgradeneeded callback');
}

function unexpectedBlockedCallback()
{
  result('fail - unexpected blocked callback');
}
