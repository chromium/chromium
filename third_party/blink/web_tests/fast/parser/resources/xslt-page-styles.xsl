<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:template match="/">
    <html>
      <head>
        <style>
          input[type="checkbox"] {
            width: 50px;
            height: 50px;
            accent-color: #ff00ff;
            border: 10px solid #000;
            border-radius: 50%;
            cursor: pointer;
            box-shadow: 10px 10px 0px #00ffff;
          }

          label {
            font-size: 3rem;
            font-family: 'Courier New', Courier, monospace;
            font-weight: 900;
            color: #ffff00;
            background-color: #0000ff;
            padding: 20px;
            border: 15px dashed #ff0000;
            text-transform: uppercase;
            letter-spacing: 5px;
            display: inline-block;
            margin: 20px;
            text-shadow: 4px 4px 0px #000;
          }

          button {
            appearance: none;
            background-color: #00ff00;
            color: #000;
            font-size: 4rem;
            padding: 40px 80px;
            border: 20px double #8a2be2;
            border-radius: 40px;
            box-shadow: inset 0 -10px 0 rgba(0,0,0,0.5), 0 15px 30px rgba(0,0,0,0.8);
            cursor: pointer;
          }

          div {
            background: linear-gradient(45deg, #ff9a9e 0%, #fecfef 99%, #fecfef 100%);
            border: 30px solid #333;
            padding: 100px;
            margin: 50px;
            border-radius: 0 100px 0 100px;
            box-shadow: -20px -20px 0px rgba(255,165,0,0.5);
          }

          p {
            font-size: 2.5rem;
            line-height: 3;
            font-family: Impact, Haettenschweiler, 'Arial Narrow Bold', sans-serif;
            color: #fff;
            background-color: #111;
            padding: 50px;
            border-left: 40px solid #00ced1;
            border-right: 40px solid #ff1493;
            text-align: justify;
            word-spacing: 20px;
          }

          a {
            color: purple;
            font-size: 30px;
            font-weight: bold;
          }
        </style>
      </head>
      <body>
        <span>hello world</span>
      </body>
    </html>
  </xsl:template>
</xsl:stylesheet>
