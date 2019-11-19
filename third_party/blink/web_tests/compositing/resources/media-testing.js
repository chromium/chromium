
if (window.testRunner)
    testRunner.waitUntilDone();

function setupVideo(videoElement, videoPath, canPlayThroughCallback)
{
    videoElement.addEventListener("canplaythrough", canPlayThroughCallback);
    videoElement.preload = "auto";
    videoElement.src = videoPath;
}
