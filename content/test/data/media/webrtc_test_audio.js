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
async function ensureSilence(audioTrack) {
  const result = await ensureSilenceOrPlayingForAudioTrack(
      audioTrack, /*checkPlaying*/ false);
  assertTrue(result);
}

async function isSilentAudioBlob(blob) {
  console.log('Blob size:' + blob.size);
  const arrayBuffer = await blob.arrayBuffer();

  const audioCtx = new AudioContext();
  const audioBuffer = await audioCtx.decodeAudioData(arrayBuffer);

  let silent = true;
  for (let ch = 0; ch < audioBuffer.numberOfChannels; ch++) {
    const channelData = audioBuffer.getChannelData(ch);
    for (let i = 0; i < channelData.length; i++) {
      if (Math.abs(channelData[i]) >
          1e-5) {  // threshold to ignore tiny float errors
        silent = false;
        break;
      }
    }
    if (!silent)
      break;
  }

  return silent;
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

  assertEquals(audioTrack.readyState, 'live');

  // Configure the analyser
  analyser.fftSize = 512;
  const dataArray = new Uint8Array(analyser.frequencyBinCount);

  // Connect the track to the analyser
  mediaStreamSource.connect(analyser);

  return new Promise((resolve) => {
    let attempts = 0;
    let silentCount = 0;

    const checkSilence = () => {
      analyser.getByteFrequencyData(dataArray);

      // Calculate average volume in decibels
      const sum = dataArray.reduce((acc, value) => acc + value, 0);
      const average = sum / dataArray.length;
      const decibels = 20 * Math.log10(average / 255);

      if (decibels < threshold) {
        silentCount++;
      }

      attempts++;

      console.log(
          'decibels: ' + decibels + ' silentCount: ' + silentCount,
          'attempts: ' + attempts);

      if (silentCount === maxAttempts) {
        console.log('Silence detected consistently.');
        close(!checkPlaying);
      } else if (attempts >= maxAttempts) {
        console.log('Playing detected consistently.');
        close(checkPlaying);
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

    function close(result) {
      audioContext.close().then(() => {
        console.log('AudioContext closed.');
        clearInterval(intervalId);
        resolve(result);
      });
    }
  });
}
