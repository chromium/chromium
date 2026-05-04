# UIA client process hashes

UIA client process histograms record the signed sparse histogram sample from
`static_cast<int>(base::PersistentHash(process_name))`, where `process_name` is
the lowercased process basename.

Keep this list ordered alphabetically by process name. Add newly identified UIA
client process names here alongside their hash values.

| Process name | Known client | `base::PersistentHash` | Sparse sample |
| --- | --- | ---: | ---: |
| `bemyeyes.exe` | Be My Eyes | `979294987` | `979294987` |
| `dolsrvcbar2.exe` | Dolphin | `1347269751` | `1347269751` |
| `grammarly.exe` | Grammarly | `3287231323` | `-1007735973` |
| `hal.exe` | Supernova | `1976295005` | `1976295005` |
| `jfw.exe` | JAWS | `1248433627` | `1248433627` |
| `magic.exe` | MAGic | `2562083169` | `-1732884127` |
| `magnify.exe` | Windows Magnifier | `3025551654` | `-1269415642` |
| `narrator.exe` | Windows Narrator | `2638653865` | `-1656313431` |
| `natspeak.exe` | Dragon NaturallySpeaking | `272349872` | `272349872` |
| `nvda.exe` | NVDA | `2313072301` | `-1981894995` |
| `nvda_nouiaccess.exe` | NVDA without UIAccess | `1304689049` | `1304689049` |
| `sa.exe` | SA | `1602550339` | `1602550339` |
| `snova.exe` | SuperNova Access Suite | `3637293433` | `-657673863` |
| `voiceaccess.exe` | Voice Access | `3295411586` | `-999555710` |
| `wineyes.exe` | Window-Eyes | `621100454` | `621100454` |
| `zt.exe` | ZoomText | `2323185757` | `-1971781539` |
