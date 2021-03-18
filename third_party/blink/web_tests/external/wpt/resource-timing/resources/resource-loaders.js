const load = {
  // Returns a promise that settles once the given path has been fetched as an
  // image resource.
  image: path => {
    return new Promise(resolve => {
      const img = new Image();
      img.onload = img.onerror = resolve;
      img.src = path;
    });
  },

  // Returns a promise that settles once the given path has been fetched as a
  // font resource.
  font: path => {
    const div = document.createElement('div');
    div.innerHTML = `
      <style>
      @font-face {
          font-family: ahem;
          src: url('${path}');
      }
      </style>
      <div style="font-family: ahem;">This fetches ahem font.</div>
    `;
    document.body.appendChild(div);
    return document.fonts.ready.then(() => {
      document.body.removeChild(div);
    });
  }
};
