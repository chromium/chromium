// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Audio test utilities.

// Uses WebAudio's analyser of the on peerConnection's media stream to find
// out whether audio is muted on the connection.
// Note this does not necessarily mean the audio is actually
// playing out (for instance if there's a bug in the WebRTC web media player).
async function ensureAudioPlaying(audioTrack) {
  const result = await ensureSilenceOrPlayingForAudioTrack(
      audioTrack, /*checkPlaying*/ true);
  assertTrue(result);
}

// Uses WebAudio's analyser of the on peerConnection's media stream to find
// out whether audio is muted on the connection.
async function ensureAudioSilence(audioTrack) {
  const result = await ensureSilenceOrPlayingForAudioTrack(
      audioTrack, /*checkPlaying*/ false);
  assertTrue(result);
}

/**
 * @private
 */
function ensureSilenceOrPlaying(peerConnection, checkPlaying) {
  const remoteAudioTrack = getRemoteAudioTrack(peerConnection);
  assertTrue(remoteAudioTrack);
  return ensureSilenceOrPlayingForAudioTrack(remoteAudioTrack, checkPlaying);
}

/**
 * @private
 */
async function ensureSilenceOrPlayingForAudioTrack(audioTrack, checkPlaying) {
  const threshold = -50;
  const checkInterval = 100;
  const maxAttempts = 20;
  const audioContext = new AudioContext();
  const mediaStreamSource =
      audioContext.createMediaStreamSource(new MediaStream([audioTrack]));
  const analyser = audioContext.createAnalyser();

  console.log('checkPlaying: ' + checkPlaying);

  // Configure the analyser
  analyser.fftSize = 512;
  const dataArray = new Uint8Array(analyser.frequencyBinCount);

  // Connect the track to the analyser
  mediaStreamSource.connect(analyser);

  return new Promise((resolve) => {
    let detectionCount = 0;
    let attempts = 0;
    const checkSilence = () => {
      analyser.getByteFrequencyData(dataArray);

      // Calculate average volume in decibels
      const sum = dataArray.reduce((acc, value) => acc + value, 0);
      const average = sum / dataArray.length;
      const decibels = 20 * Math.log10(average / 255);

      if (checkPlaying && decibels > threshold) {
        detectionCount++;
      } else if (!checkPlaying && decibels < threshold) {
        detectionCount++;
      }

      if (detectionCount == 1) {
        attempts = 0;
      }
      attempts++;

      console.log(
          'decibels: ' + decibels + ' detectionCount: ' + detectionCount);

      if (detectionCount === maxAttempts) {
        // Once silence or playing is detected, it keeps the state.
        assertEquals(attempts, maxAttempts);

        console.log(
            checkPlaying ? 'Playing detected consistently.' :
                           'Silence detected consistently.');
        close();
      }
    };

    // Periodically check for silence
    const intervalId = setInterval(checkSilence, checkInterval);

    // Stop the check if the track ends
    audioTrack.onended = () => {
      console.log('Audio track ended.');
      clearInterval(intervalId);
      resolve(false);
    };

    function close() {
      audioContext.close().then(() => {
        console.log('AudioContext closed.');
        clearInterval(intervalId);
        resolve(true);
      });
    }
  });
}
