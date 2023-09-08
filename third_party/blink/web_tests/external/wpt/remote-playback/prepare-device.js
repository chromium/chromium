var startButton = document.getElementById("start-button");
startButton.onclick = function () {
  document.getElementById("prep").style.display = "none";
  document.getElementById("pick-device").style.display = "block";
};
var promptPrepButton = document.getElementById("prompt-button-prep");
promptPrepButton.onclick = function () {
  v.remote
    .prompt()
    .then(() => {})
    .catch(() => {});
};
