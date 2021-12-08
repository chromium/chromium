// Set permissions for camera and microphone using Web Driver
// Status can be one of "granted" or "denied"
// Scope take values from permission names
async function setMediaPermission(status="granted", scope=["camera", "microphone"]) {
  try {
    for (let s of scope) {
      await test_driver.set_permission({ name: s }, status, true);
    }
  } catch (e) {
    if (!(typeof e === "string" && e.match(/set_permission not implemented/))) {
      throw e
    }
    // Web Driver not implemented action
    // will default to granted state for FF and Safari
  }
}
