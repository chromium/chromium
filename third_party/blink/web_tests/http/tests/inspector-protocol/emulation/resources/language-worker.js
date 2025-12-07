onmessage = function(e) {
  postMessage({language: navigator.language, languages: navigator.languages});
  if (e.data === 'close') {
    close();
  }
};
