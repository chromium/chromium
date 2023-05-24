/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
(function() {

window.Leaf = Utilities.createSubclass(Particle,
    function(stage)
    {
        this.element = document.createElement("img");
        this.element.setAttribute("src", Stage.randomElementInArray(stage.images).src);
        stage.element.appendChild(this.element);

        Particle.call(this, stage);
    }, {

    sizeMinimum: 20,
    sizeRange: 30,
    usesOpacity: true,

    reset: function()
    {
        Particle.prototype.reset.call(this);
        this.element.style.width = this.size.x + "px";
        this.element.style.height = this.size.y + "px";

        if (this.usesOpacity) {
            this._opacity = .01;
            this._opacityRate = 0.02 * Stage.random(1, 6);
        } else
            this._life = Stage.randomInt(20, 100);

        this._position = new Point(Stage.random(0, this.maxPosition.x), Stage.random(-this.size.height, this.maxPosition.y));
        this._velocity = new Point(Stage.random(-6, -2), .1 * this.size.y + Stage.random(-1, 1));
    },

    animate: function(timeDelta)
    {
        this.rotater.next(timeDelta);

        this._position.x += this._velocity.x + 8 * this.stage.focusX;
        this._position.y += this._velocity.y;

        if (this.usesOpacity) {
            this._opacity += this._opacityRate;
            if (this._opacity > 1) {
                this._opacity = 1;
                this._opacityRate *= -1;
            } else if (this._opacity < 0 || this._position.y > this.stage.size.height)
                this.reset();
        } else {
            this._life--;
            if (!this._life || this._position.y > this.stage.size.height)
                this.reset();
        }

        if (this._position.x < -this.size.width || this._position.x > this.stage.size.width)
            this._position.x = this._position.x - Math.sign(this._position.x) * (this.size.width + this.stage.size.width);
        this.move();
    },

    move: function()
    {
        this.element.style.transform = "translate(" + this._position.x + "px, " + this._position.y + "px)" + this.rotater.rotateZ();
        this.element.style.opacity = this._opacity;
    }
});

Utilities.extendObject(ParticlesStage.prototype, {

    imageSrcs: [
        "compass",
        "console",
        "contribute",
        "debugger",
        "inspector",
        "layout",
        "performance",
        "script",
        "shortcuts",
        "standards",
        "storage",
        "styles",
        "timeline"
    ],
    images: [],

    initialize: function(benchmark)
    {
        Stage.prototype.initialize.call(this, benchmark);

        var lastPromise;
        var images = this.images;
        this.imageSrcs.forEach(function(imageSrc) {
            var promise = this._loadImage("../master/resources/" + imageSrc + "100.png"); // nocheck
            if (!lastPromise)
                lastPromise = promise;
            else {
                lastPromise = lastPromise.then(function(img) {
                    images.push(img);
                    return promise;
                });
            }
        }, this);

        lastPromise.then(function(img) {
            images.push(img);
            benchmark.readyPromise.resolve();
        });
    },

    _loadImage: function(src) {
        var img = new Image;
        var promise = new SimplePromise;

        img.onload = function(e) {
            promise.resolve(e.target);
        };

        img.src = src;
        return promise;
    },

    animate: function(timeDelta)
    {
        this.focusX = 0.5 + 0.5 * Math.sin(Stage.dateFractionalValue(10000) * Math.PI * 2);
        timeDelta /= 4;
        this.particles.forEach(function(particle) {
            particle.animate(timeDelta);
        });
    },

    createParticle: function()
    {
        return new Leaf(this);
    },

    willRemoveParticle: function(particle)
    {
        particle.element.remove();
    }
});

var LeavesBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new ParticlesStage(), options);
    }, {

    waitUntilReady: function() {
        this.readyPromise = new SimplePromise;
        return this.readyPromise;
    }

});

window.benchmarkClass = LeavesBenchmark;

})();
