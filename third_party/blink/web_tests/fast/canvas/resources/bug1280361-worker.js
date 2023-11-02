self.onmessage = e => {
  const bitmap = e.data;
  const canvas = new OffscreenCanvas(bitmap.width, bitmap.height);
  const gl = canvas.getContext('webgl');
  const texture = gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D, texture);
  gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, bitmap);
  bitmap.close();
  self.postMessage('Done');
}
