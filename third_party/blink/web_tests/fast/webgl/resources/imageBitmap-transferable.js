self.onmessage = function(e) {
    createImageBitmap(e.data, {imageOrientation: "from-image", premultiplyAlpha: "none"}).then(imageBitmap => {
        postMessage(imageBitmap, [imageBitmap]);
    });
};
