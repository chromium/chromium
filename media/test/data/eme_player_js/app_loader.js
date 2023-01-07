// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Adds a Javascript source tag to the document.
function addScriptTag(src) {
  document.write(
      '<script type="text/javascript" src="eme_player_js/' + src +
      '"></script>');
}

// Load all the dependencies for the app.
addScriptTag('globals.js');
addScriptTag('utils.js');
addScriptTag('test_config.js');
addScriptTag('fps_observer.js');
addScriptTag('media_source_utils.js');
addScriptTag('player_utils.js');
addScriptTag('clearkey_player.js');
addScriptTag('widevine_player.js');
addScriptTag('unit_test_player.js');
addScriptTag('eme_app.js');
addScriptTag('mse_player_utils.js');
