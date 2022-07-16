#Audio service

Provides core audio functionality: audio device access and enumeration.

Runs
* In a separate process on Windows, Mac (sandboxed) and Linux (unsandboxed);
* In the browser process on other platforms.

Can be accessed from trusted processes only (the browser process and certains utility processes).
Use audio::CreateInputDevice() for the mic capture, and audio::OutputDevice for the playback.

Untrusted processes should use media::AudioInputDevice and media::AudioOutputDevice correspondingly, 
which will take care of device authorization.

[Design doc](https://docs.google.com/document/d/1s_Fd1WRDdpb5n6C2MSJjeC3fis6hULZwfKMeDd4K5tI/edit?usp=sharing)